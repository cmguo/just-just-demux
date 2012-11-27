// TsDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/ts/TsDemuxer.h"
#include "ppbox/demux/ts/TsStream.h"
using namespace ppbox::demux::error;

#include <ppbox/avformat/codec/avc/AvcNaluHelper.h>
#include <ppbox/avformat/codec/avc/AvcNalu.h>
using namespace ppbox::avformat;

#include <util/serialization/Array.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
using namespace framework::logger;

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.TsDemuxer", Debug)

namespace ppbox
{
    namespace demux
    {

        TsDemuxer::TsDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : Demuxer(io_svc, buf)
            , archive_(buf)
            , open_step_(size_t(-1))
            , header_offset_(0)
            , parse_offset_(0)
            , pes_size_(0)
            , pes_left_(0)
            , pes_frame_offset_(0)
            , parse_offset2_(0)
            , time_valid_(false)
            , timestamp_offset_ms_(0)
            , current_time_(0)
        {
        }

        TsDemuxer::~TsDemuxer()
        {
        }

        error_code TsDemuxer::open(
            error_code & ec)
        {
            open_step_ = 0;
            parse_offset_ = parse_offset2_ = header_offset_ = 0;
            time_valid_ = false;
            is_open(ec);
            return ec;
        }

        bool TsDemuxer::is_open(
            error_code & ec)
        {
            if (open_step_ == 3) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (size_t)-1) {
                ec = not_open;
                return false;
            }

            assert(archive_);
            archive_.seekg(parse_offset_, std::ios_base::beg);
            assert(archive_);

            if (open_step_ == 0) {
                while (get_packet(pkt_, ec)) {
                    if (pkt_.pid != TsPid::pat) {
                        skip_packet();
                        continue;
                    }
                    PatPayload pat(pkt_);
                    archive_ >> pat;
                    if (!archive_) {
                        if (archive_.failed()) {
                            ec = error::bad_file_format;
                        } else {
                            ec = error::file_stream_error;
                        }
                        archive_.clear();
                        break;
                    }
                    pat_ = pat.sections[0].programs[0];
                    parse_offset_ = archive_.tellg();
                    open_step_ = 1;
                    break;
                }
            }

            if (open_step_ == 1) {
                while (get_packet(pkt_, ec)) {
                    if (pkt_.pid != pat_.map_id) {
                        skip_packet();
                        continue;
                    }
                    PmtPayload pmt(pkt_);
                    archive_ >> pmt;
                    if (!archive_) {
                        if (archive_.failed()) {
                            ec = error::bad_file_format;
                        } else {
                            ec = error::file_stream_error;
                        }
                        archive_.clear();
                        break;
                    }
                    parse_offset_ = archive_.tellg();
                    pmt_ = pmt.sections[0];
                    for (size_t i = 0; i < pmt_.streams.size(); ++i) {
                        if (stream_map_.size() <= pmt_.streams[i].elementary_pid)
                            stream_map_.resize(pmt_.streams[i].elementary_pid + 1, (size_t)-1);
                        stream_map_[pmt_.streams[i].elementary_pid] = streams_.size();
                        streams_.push_back(TsStream(pmt_.streams[i]));
                    }
                    open_step_ = 2;
                    header_offset_ = parse_offset_;
                    break;
                }
            }

            if (open_step_ == 2) {
                while (get_pes(ec)) {
                    TsStream & stream = streams_[stream_map_[pes_pid_]];
                    if (stream.ready) {
                        free_pes();
                        continue;
                    }
                    if (!time_valid_) {
                        if (pes_.PTS_DTS_flags == 0x02) {
                            timestamp_offset_ms_ = pes_.pts_bits.value();
                            time_valid_ = true;
                        } else if (pes_.PTS_DTS_flags == 0x03) {
                            timestamp_offset_ms_ = pes_.dts_bits.value();
                            time_valid_ = true;
                        }
                    }
                    std::vector<boost::uint8_t> data(pes_size_, 0);
                    boost::uint32_t read_offset = 0;
                    for (size_t i = 0; i < pes_payloads_.size(); ++i) {
                        archive_.seekg(pes_payloads_[i].offset, std::ios::beg);
                        assert(archive_);
                        archive_ >> framework::container::make_array(&data[read_offset], pes_payloads_[i].size);
                        assert(archive_);
                        read_offset += pes_payloads_[i].size;
                    }
                    free_pes();
                    stream.set_pes(data);
                    bool ready = true;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        if (!streams_[i].ready) {
                            ready = false;
                            break;
                        }
                    }
                    if (ready) {
                        archive_.seekg(header_offset_, std::ios_base::beg);
                        parse_offset_ = parse_offset2_ = header_offset_;
                        for (size_t i = 0; i < streams_.size(); ++i) {
                            streams_[i].index = i;
                            streams_[i].start_time = timestamp_offset_ms_;
                        }
                        open_step_ = 3;
                        break;
                    }
                }
            }

            if (ec) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                return false;
            } else {
                on_open();
                assert(open_step_ == 3);
                return true;
            }
        }

        bool TsDemuxer::is_open(
            error_code & ec) const
        {
            if (open_step_ == 3) {
                ec = error_code();
                return true;
            } else {
                ec = not_open;
                return false;
            }
        }

        error_code TsDemuxer::close(
            error_code & ec)
        {
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t TsDemuxer::seek(
            boost::uint64_t & time, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (is_open(ec)) {
                ec.clear();
                time = 0;
                parse_offset_ = header_offset_;
                return header_offset_;
            } else {
                return 0;
            }
        }

        boost::uint64_t TsDemuxer::get_duration(
            error_code & ec) const
        {
            ec = error::not_support;
            return 0;
        }

        size_t TsDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec))
                return streams_.size();
            return 0;
        }

        error_code TsDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            error_code & ec) const
        {
            if (is_open(ec)) {
                if (index >= stream_map_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_[index];
                }
            }
            return ec;
        }

        error_code TsDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            if (get_pes(ec)) {
                TsStream & stream = streams_[stream_map_[pes_pid_]];
                sample.itrack = stream.index;
                sample.idesc = 0;
                sample.flags = 0; // TODO: is_sync
                if (stream.type == MEDIA_TYPE_VIDE && is_sync_frame()) {
                    sample.flags |= Sample::sync;
                }
                if (pes_.PTS_DTS_flags == 3) {
                    sample.dts = pes_.dts_bits.value(); // timestamp_.transfer((boost::uint64_t)flv_tag_.Timestamp);
                    sample.cts_delta = (boost::uint32_t)(pes_.pts_bits.value() - pes_.dts_bits.value());
                } else if (pes_.PTS_DTS_flags == 2) {
                    pes_.dts_bits = pes_.pts_bits;
                    sample.dts = pes_.pts_bits.value(); // timestamp_.transfer((boost::uint64_t)flv_tag_.Timestamp);
                    sample.cts_delta = 0;
                }
                sample.duration = 0;
                Demuxer::adjust_timestamp(sample);
                sample.size = pes_size_;
                sample.blocks.clear();
                sample.blocks.swap(pes_payloads_);
                current_time_ = sample.time;
                free_pes();
            }
            return ec;
        }

        boost::uint64_t TsDemuxer::get_cur_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            return current_time_;
        }

        boost::uint64_t TsDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            archive_.seekg(parse_offset2_, std::ios::beg);
            assert(archive_);
            while (get_packet(pkt2_, ec)) {
                parse_offset2_ += TsPacket::PACKET_SIZE;
                archive_.seekg(parse_offset2_, std::ios::beg);
            }
            ec.clear();
            return (boost::uint64_t)pkt2_.adaptation.program_clock_reference_base << 1;
        }

        bool TsDemuxer::get_packet(
            TsPacket & pkt, 
            error_code & ec)
        {
            archive_.seekg(TsPacket::PACKET_SIZE, std::ios::cur);
            if (archive_) {
                archive_.seekg(-(int)TsPacket::PACKET_SIZE, std::ios::cur);
                assert(archive_);
                archive_ >> pkt;
                if (archive_) {
                    ec.clear();
                    return true;
                } else if (archive_.failed()) {
                    archive_.clear();
                    ec = bad_file_format;
                } else {
                    archive_.clear();
                    ec = file_stream_error;
                }
            } else {
                archive_.clear();
                ec = file_stream_error;
            }
            return false;
        }

        void TsDemuxer::skip_packet()
        {
            parse_offset_ += TsPacket::PACKET_SIZE;
            archive_.seekg(parse_offset_, std::ios::beg);
            assert(archive_);
        }

        bool TsDemuxer::get_pes(
            boost::system::error_code & ec)
        {
            while (get_packet(pkt_, ec)) {
                if (pkt_.pid >= stream_map_.size() || stream_map_[pkt_.pid] == (size_t)-1) {
                    skip_packet();
                    continue;
                }
                boost::uint64_t offset = archive_.tellg();
                boost::uint32_t size = pkt_.payload_size();
                if (pkt_.payload_uint_start_indicator == 1) {
                    if (pes_size_ == 0) {
                        archive_ >> pes_;
                        assert(archive_);
                        pes_pid_ = pkt_.pid;
                        pes_size_ = pes_left_ = pes_.payload_length();
                        boost::uint64_t offset1 = archive_.tellg();
                        size -= (boost::uint32_t)(offset1 - offset);
                        offset = offset1;
                    } else {
                        if (pes_left_ != 0) {
                            LOG_WARN("[get_sample] payload size less than expect, size = " << pes_size_ << ", less = " << pes_left_);
                            pes_size_ -= pes_left_;
                            pes_left_ = 0;
                        }
                        parse_offset_ -= TsPacket::PACKET_SIZE;
                        break;
                    }
                }
                pes_payloads_.push_back(FileBlock(offset, size));
                if (pes_left_ == 0) {
                    pes_size_ += size;
                    skip_packet();
                    continue;
                } else {
                    if (pes_left_ > size) {
                        pes_left_ -= size;
                        skip_packet();
                        continue;
                    }
                    if (pes_left_ < size) {
                        LOG_WARN("[get_sample] payload size more than expect, size = " << pes_size_ << ", more = " << size - pes_left_);
                        pes_size_ += size - pes_left_;
                        pes_left_ = 0;
                    }
                }
                break;
            }
            return !ec;
        }

        bool TsDemuxer::is_sync_frame()
        {
            using namespace ppbox::avformat;
            using namespace framework::container;

            if (pes_frame_offset_ > 0) {
                boost::uint8_t data[5];
                boost::uint32_t frame_offset = pes_frame_offset_;
                boost::uint32_t read_size = 0;
                for (size_t i = 0; i < pes_payloads_.size() && read_size; ++i) {
                    if (frame_offset < pes_payloads_[i].size) {
                        archive_.seekg(pes_payloads_[i].offset + frame_offset, std::ios::beg);
                        assert(archive_);
                        boost::uint32_t read_size2 = 5 - read_size;
                        if (frame_offset + read_size2 > pes_payloads_[0].size) {
                            read_size2 = pes_payloads_[i].size - frame_offset;
                        }
                        archive_ >> make_array(data + read_size, read_size2);
                        read_size -= read_size2;
                        frame_offset = 0;
                    } else {
                        frame_offset -= pes_payloads_[i].size;
                    }
                }
                if (read_size == 0 
                    && *(boost::uint32_t *)data == MAKE_FOURC_TYPE(0, 0, 0, 1)) {
                        NaluHeader h(data[4]);
                        if (h.nal_unit_type == 1) {
                            return false;
                        } else if (h.nal_unit_type == 5) {
                            return true;
                        }
                }
            }

            std::vector<boost::uint8_t> data(pes_size_, 0);
            boost::uint32_t read_offset = 0;
            for (size_t i = 0; i < pes_payloads_.size(); ++i) {
                archive_.seekg(pes_payloads_[i].offset, std::ios::beg);
                assert(archive_);
                archive_ >> make_array(&data[read_offset], pes_payloads_[i].size);
                assert(archive_);
                read_offset += pes_payloads_[i].size;
            }
            AvcNaluHelper helper;
            boost::uint8_t frame_type = helper.get_frame_type_from_stream(data, &pes_frame_offset_);
            if (frame_type == 1) {
                return false;
            } else if (frame_type == 5) {
                return true;
            } else {
                return false;
            }
        }

        void TsDemuxer::free_pes()
        {
            pes_pid_ = 0;
            pes_size_ = pes_left_ = 0;
            pes_payloads_.clear();
            skip_packet();
        }

    }
}

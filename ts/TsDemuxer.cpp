// TsDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/ts/TsDemuxer.h"
#include "ppbox/demux/ts/TsStream.h"
using namespace ppbox::demux::error;

using namespace ppbox::avformat;

#include <util/serialization/Array.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.TsDemuxer", framework::logger::Debug)

#include "ppbox/demux/ts/PesParse.h"

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
            , pes_index_(size_t(-1))
            , min_offset_(0)
            , parse_offset2_(0)
            , timestamp_offset_ms_(boost::uint64_t(-1))
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
            timestamp_offset_ms_ = boost::uint64_t(-1);
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
                    pes_parses_.resize(streams_.size());
                    open_step_ = 2;
                    header_offset_ = parse_offset_;
                    break;
                }
            }

            if (open_step_ == 2) {
                while (get_pes(ec)) {
                    TsStream & stream = streams_[pes_index_];
                    if (stream.ready) {
                        free_pes();
                        continue;
                    }
                    PesParse & parse = pes_parses_[pes_index_];
                    if (timestamp_offset_ms_ > parse.dts()) {
                        timestamp_offset_ms_ = parse.dts(); // 先不转换精度 / (TsPacket::TIME_SCALE / 1000);
                    }
                    std::vector<boost::uint8_t> data;
                    parse.get_data(data, archive_);
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
                        parse_offset_ = parse_offset2_ = min_offset_ = header_offset_;
                        for (size_t i = 0; i < streams_.size(); ++i) {
                            streams_[i].index = i;
                            streams_[i].start_time = timestamp_offset_ms_;
                            std::vector<ppbox::avformat::FileBlock> payloads;
                            pes_parses_[i].clear(payloads);
                        }
                        timestamp_offset_ms_ /= (TsPacket::TIME_SCALE / 1000); // 现在转换精度
                        open_step_ = 3;
                        break;
                    }
                }
            }

            if (ec) {
                archive_.seekg(header_offset_, std::ios_base::beg);
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
            for (size_t i = 0; i < streams_.size(); ++i) {
                streams_[i].clear();
                std::vector<ppbox::avformat::FileBlock> payloads;
                pes_parses_[i].clear(payloads);
            }
            streams_.clear();
            pes_parses_.clear();
            stream_map_.clear();
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
            return ppbox::data::invalid_size;
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
            archive_.seekg(parse_offset_, std::ios::beg);
            assert(archive_);
            if (get_pes(ec)) {
                TsStream & stream = streams_[pes_index_];
                PesParse & parse = pes_parses_[pes_index_];
                sample.itrack = stream.index;
                sample.flags = 0; // TODO: is_sync
                if (stream.type == MEDIA_TYPE_VIDE && parse.is_sync_frame(archive_)) {
                    sample.flags |= Sample::sync;
                }
                sample.dts = time_dts_.transfer(parse.dts());
                sample.cts_delta = parse.cts_delta();
                sample.duration = 0;
                Demuxer::adjust_timestamp(sample);
                sample.size = parse.size();
                free_pes(sample.blocks);
                current_time_ = sample.time;
            }
            archive_.seekg(min_offset_, std::ios::beg);
            assert(archive_);
            return ec;
        }

        boost::uint64_t TsDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t pcr = time_pcr_.transfer(
                ((boost::uint64_t)pkt_.adaptation.program_clock_reference_base) << 1);
            pcr /= (TsPacket::TIME_SCALE / 1000);
            return pcr > timestamp_offset_ms_ ? pcr  - timestamp_offset_ms_ : 0;
        }

        boost::uint64_t TsDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t beg = archive_.tellg();
            archive_.seekg(0, std::ios::end);
            assert(archive_);
            boost::uint64_t end = archive_.tellg();
            assert(end > TsPacket::PACKET_SIZE);
            end = (end / TsPacket::PACKET_SIZE) * TsPacket::PACKET_SIZE;
            if (parse_offset2_ + TsPacket::PACKET_SIZE * 20 < end) {
                parse_offset2_ = end - TsPacket::PACKET_SIZE * 20;
            }
            archive_.seekg(parse_offset2_, std::ios::beg);
            assert(archive_);
            while (parse_offset2_ <= end && get_packet(pkt2_, ec)) {
                parse_offset2_ += TsPacket::PACKET_SIZE;
                archive_.seekg(parse_offset2_, std::ios::beg);
            }
            ec.clear();
            archive_.seekg(beg, std::ios::beg);
            boost::uint64_t pcr = time_pcr2_.transfer(
                ((boost::uint64_t)pkt2_.adaptation.program_clock_reference_base) << 1);
            pcr /= (TsPacket::TIME_SCALE / 1000);
            return pcr > timestamp_offset_ms_ ? pcr  - timestamp_offset_ms_ : 0;
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
                pes_index_ = stream_map_[pkt_.pid];
                std::pair<bool, bool> res = 
                    pes_parses_[pes_index_].add_packet(pkt_, archive_, ec);
                if (res.second) {
                    parse_offset_ -= TsPacket::PACKET_SIZE;
                }
                if (res.first) {
                    break;
                }
                skip_packet();
            }
            return !ec;
        }

        void TsDemuxer::free_pes()
        {
            std::vector<ppbox::avformat::FileBlock> payloads;
            free_pes(payloads);
        }

        void TsDemuxer::free_pes(
            std::vector<ppbox::avformat::FileBlock> & payloads)
        {
            PesParse & parse = pes_parses_[pes_index_];
            pes_index_ = size_t(-1);
            parse.clear(payloads);
            min_offset_ = parse_offset_;
            for (size_t i = 0; i < pes_parses_.size(); ++i) {
                boost::uint64_t min_offset = pes_parses_[i].min_offset();
                if(min_offset_ > min_offset) {
                    min_offset_ = min_offset;
                }
            }
            skip_packet();
        }

    }
}

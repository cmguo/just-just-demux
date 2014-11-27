// PsDemuxer.cpp

#include "just/demux/Common.h"
#include "just/demux/basic/mp2/PsDemuxer.h"
#include "just/demux/basic/mp2/PsStream.h"
#include "just/demux/basic/JointContext.h"
#include "just/demux/base/DemuxError.h"
using namespace just::demux::error;

#include <just/avformat/mp2/Mp2Enum.h>
using namespace just::avformat;
using namespace just::avformat::error;

#include <util/serialization/Array.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/LogicError.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.PsDemuxer", framework::logger::Warn)

namespace just
{
    namespace demux
    {

        PsDemuxer::PsDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_(size_t(-1))
            , header_offset_(0)
        {
        }

        PsDemuxer::~PsDemuxer()
        {
        }

        error_code PsDemuxer::open(
            error_code & ec)
        {
            if (streams_.empty()) {
                open_step_ = 0;
                parse_.offset = parse2_.offset = header_offset_ = 0;
            } else {
                open_step_ = 2;
            }
            is_open(ec);
            return ec;
        }

        bool PsDemuxer::is_open(
            error_code & ec)
        {
            if (open_step_ == 2) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (size_t)-1) {
                ec = error::not_open;
                return false;
            }

            assert(archive_);
            archive_.seekg(parse_.offset, std::ios_base::beg);
            assert(archive_);

            if (open_step_ == 0) {
                archive_ >> parse_.pkt;
                if (archive_) {
                    if (!parse_.pkt.system_headers.empty()) {
                        std::vector<PsSystemHeader::Stream> & streams = 
                            parse_.pkt.system_headers[0].streams;
                        for (size_t i = 0; i < streams.size(); ++i) {
                            PsSystemHeader::Stream const & stream = streams[i];
                            if (stream.stream_id < Mp2StreamId::audio_base 
                                && stream.stream_id >= Mp2StreamId::ecm_stream) {
                                    continue;
                            }
                            if (stream_map_.size() <= stream.stream_id)
                                stream_map_.resize(stream.stream_id + 1, (size_t)-1);
                            stream_map_[stream.stream_id] = streams_.size();
                            streams_.push_back(PsStream(stream));
                            streams_[i].index = (boost::uint32_t)stream_map_[stream.stream_id];
                            streams_[i].start_time = parse_.time_pcr.current();
                        }
                        header_offset_ = parse_.offset;
                        open_step_ = 1;
                    }
                } else {
                    if (archive_.failed()) {
                        ec = bad_media_format;
                    } else {
                        ec = file_stream_error;
                    }
                    archive_.clear();
                }
            }

            if (open_step_ == 1) {
                while (get_pes(ec)) {
                    if (parse_.pes.stream_id >= stream_map_.size() 
                        || stream_map_[parse_.pes.stream_id] == (size_t)-1) {
                            continue;
                    }
                    PsStream & stream = streams_[stream_map_[parse_.pes.stream_id]];
                    if (stream.ready) {
                        continue;
                    }
                    std::vector<boost::uint8_t> data(parse_.size());
                    archive_.seekg(parse_.pes_data_offset, std::ios::beg);
                    archive_ >> framework::container::make_array(&data[0], data.size());
                    stream.set_pes(data);
                    bool ready = true;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        if (!streams_[i].ready) {
                            ready = false;
                            break;
                        }
                    }
                    if (ready) {
                        parse_.offset = header_offset_;
                        archive_.seekg(parse_.offset, std::ios_base::beg);
                        timestamp().max_delta(1000);
                        open_step_ = 2;
                        break;
                    }
                }
            }

            if (ec) {
                return false;
            } else {
                assert(open_step_ == 2);
                on_open();
                return true;
            }
        }

        bool PsDemuxer::is_open(
            error_code & ec) const
        {
            if (open_step_ == 2) {
                ec = error_code();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        error_code PsDemuxer::close(
            error_code & ec)
        {
            if (open_step_ == 2) {
                on_close();
            }
            streams_.clear();
            stream_map_.clear();
            parse_.offset = 0;
            parse2_.offset = 0;
            header_offset_ = 0;
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t PsDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (is_open(ec)) {
                ec.clear();
                //parse_.offset = header_offset_;
                //parse_.time_pcr = streams_[0].start_time;
                //parse2_ = parse_;
                //for (size_t i = 0; i < pes_parses_.size(); ++i) {
                //    std::vector<just::data::DataBlock> payloads;
                //    pes_parses_[i].clear(payloads);
                //}
                //dts.assign(dts.size(), streams_[0].start_time); // TO BE FIXED
                return header_offset_;
            } else {
                return 0;
            }
        }

        boost::uint64_t PsDemuxer::get_duration(
            error_code & ec) const
        {
            ec = framework::system::logic_error::not_supported;
            return just::data::invalid_size;
        }

        size_t PsDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec))
                return streams_.size();
            return 0;
        }

        error_code PsDemuxer::get_stream_info(
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

        error_code PsDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            archive_.seekg(parse_.offset, std::ios::beg);
            assert(archive_);
            while (get_pes(ec)) {
                if (parse_.pes.stream_id >= stream_map_.size() 
                    || stream_map_[parse_.pes.stream_id] == (size_t)-1) {
                        continue;
                }
                PsStream & stream = streams_[stream_map_[parse_.pes.stream_id]];
                BasicDemuxer::begin_sample(sample);
                sample.itrack = stream.index;
                sample.flags = 0; // TODO: is_sync
                //if (stream.type == StreamType::VIDE && parse.is_sync_frame(archive_)) {
                //    sample.flags |= Sample::f_sync;
                //}
                sample.dts = parse_.dts();
                sample.cts_delta = parse_.cts_delta();
                sample.duration = 0;
                sample.size = parse_.size();
                sample.stream_info = &stream;
                BasicDemuxer::push_data(parse_.data_offset(), parse_.size());
                BasicDemuxer::end_sample(sample);
                break;
            }
            assert(archive_);
            return ec;
        }

        boost::uint32_t PsDemuxer::probe(
            boost::uint8_t const * hbytes, 
            size_t hsize)
        {
            boost::uint32_t scope = 0;
            if (hsize >= 4 
                && hbytes[0] == 0 
                && hbytes[1] == 0
                && hbytes[2] == 1
                && hbytes[3] == 0xba) {
                scope = SCOPE_MAX / 2;
            }
            return scope;
        }

        boost::uint64_t PsDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t pcr = parse_.time_pcr.current();
            return timestamp().const_adjust(0, pcr);
        }

        boost::uint64_t PsDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t beg = archive_.tellg();
            archive_.seekg(0, std::ios::end);
            assert(archive_);
            //boost::uint64_t end = archive_.tellg();
            //assert(end > PsPacket::PACKET_SIZE);
            //end = (end / PsPacket::PACKET_SIZE) * PsPacket::PACKET_SIZE;

            //if (parse2_.offset < parse_.offset) {
            //    parse2_ = parse_;
            //}

            //if (parse2_.offset > end) { // 有可能外面数据清除了
            //    parse2_.offset = 0;
            //    parse2_.time_pcr = streams_[0].start_time;
            //}

            //if (parse2_.offset + PsPacket::PACKET_SIZE * 20 < end) {
            //    parse2_.offset = end - PsPacket::PACKET_SIZE * 20;
            //}
            //archive_.seekg(parse2_.offset, std::ios::beg);
            //assert(archive_);
            //while (parse2_.offset < end && get_packet(parse2_, ec)) {
            //    skip_packet(parse2_);
            //    if (parse2_.pkt.has_pcr()) {
            //        break;
            //    }
            //}
            archive_.seekg(beg, std::ios::beg);
            ec.clear();
            boost::uint64_t pcr = parse2_.time_pcr.current();
            return timestamp().const_adjust(0, pcr);
        }

        bool PsDemuxer::get_pes(
            boost::system::error_code & ec)
        {
            if (parse_.pkt.pack_start_code == 0) {
                archive_ >> parse_.pkt;
                if (!archive_) {
                    if (archive_.failed()) {
                        if (parse_.pkt.pack_start_code == 0x000001b9) {
                            ec = end_of_stream;
                        } else {
                            ec = bad_media_format;
                        }
                    } else {
                        ec = file_stream_error;
                    }
                    archive_.clear();
                    return false;
                }
                parse_.time_pcr.transfer(parse_.pkt.pcr());
                parse_.offset = archive_.tellg();
            }
            archive_ >> parse_.pes;
            if (archive_) {
                parse_.pes_data_offset = archive_.tellg();
                archive_.seekg(parse_.pes.payload_length(), std::ios::cur);
                if (!archive_) {
                    ec = file_stream_error;
                    archive_.clear();
                    return false;
                }
                parse_.offset = archive_.tellg();
                if (parse_.pes.pts_bits.flag == 3) {
                    LOG_ERROR("[get_pes] pts: " << parse_.pes.pts_bits.value() 
                        << ", dts: " << parse_.pes.dts_bits.value() 
                        << ", data: " << parse_.pes_data_offset << " - " << parse_.offset);
                } else if (parse_.pes.pts_bits.flag == 2) {
                    LOG_ERROR("[get_pes] pts: " << parse_.pes.pts_bits.value() 
                        << ", data: " << parse_.pes_data_offset << " - " << parse_.offset);
                } else {
                    LOG_ERROR("[get_pes] data: " << parse_.pes_data_offset << " - " << parse_.offset);
                }
                return true;
            }
            if (archive_.failed()) {
                archive_.clear();
                archive_.seekg(parse_.offset, std::ios::beg);
                parse_.pkt.pack_start_code = 0;
                return get_pes(ec);
            } else {
                ec = file_stream_error;
                archive_.clear();
                return false;
            }
        }

    }
}

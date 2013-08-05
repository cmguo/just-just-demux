// TsDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/ts/TsDemuxer.h"
#include "ppbox/demux/basic/ts/TsStream.h"
#include "ppbox/demux/basic/JointContext.h"
#include "ppbox/demux/base/DemuxError.h"

#include <ppbox/avformat/ts/TsEnum.h>
using namespace ppbox::avformat;

#include <util/serialization/Array.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.TsDemuxer", framework::logger::Warn)

#include "ppbox/demux/basic/ts/PesParse.h"
#include "ppbox/demux/basic/ts/TsJointData.h"
#include "ppbox/demux/basic/ts/TsJointShareInfo.h"

namespace ppbox
{
    namespace demux
    {

        TsDemuxer::TsDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_(size_t(-1))
            , header_offset_(0)
            , pes_index_(size_t(-1))
        {
        }

        TsDemuxer::~TsDemuxer()
        {
        }

        error_code TsDemuxer::open(
            error_code & ec)
        {
            if (streams_.empty()) {
                open_step_ = 0;
                parse_.offset = parse2_.offset = header_offset_ = 0;
            } else {
                open_step_ = 3;
            }
            is_open(ec);
            return ec;
        }

        bool TsDemuxer::is_open(
            error_code & ec)
        {
            if (open_step_ == 4) {
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
                while (get_packet(parse_, ec)) {
                    if (parse_.pkt.pid != TsPid::pat) {
                        skip_packet(parse_);
                        continue;
                    }
                    PatPayload pat(parse_.pkt);
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
                    parse_.offset = archive_.tellg();
                    open_step_ = 1;
                    break;
                }
            }

            if (open_step_ == 1) {
                while (get_packet(parse_, ec)) {
                    if (parse_.pkt.pid != pat_.map_id) {
                        skip_packet(parse_);
                        continue;
                    }
                    PmtPayload pmt(parse_.pkt);
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
                    parse_.offset = archive_.tellg();
                    pmt_ = pmt.sections[0];
                    for (size_t i = 0; i < pmt_.streams.size(); ++i) {
                        if (stream_map_.size() <= pmt_.streams[i].elementary_pid)
                            stream_map_.resize(pmt_.streams[i].elementary_pid + 1, (size_t)-1);
                        stream_map_[pmt_.streams[i].elementary_pid] = streams_.size();
                        streams_.push_back(TsStream(pmt_.streams[i]));
                        streams_[i].index = i;
                    }
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        pes_parses_.push_back(PesParse(streams_[i].stream_type));
                    }
                    open_step_ = 2;
                    header_offset_ = parse_.offset;
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
                        parse_.offset = header_offset_;
                        parse_.had_pcr = false;
                        archive_.seekg(parse_.offset, std::ios_base::beg);
                        open_step_ = 3;
                        break;
                    }
                }
            }

            if (open_step_ == 3) {
                while (!parse_.had_pcr && get_packet(parse_, ec)) {
                    skip_packet(parse_);
                }
                if (parse_.had_pcr) {
                    parse2_ = parse_;
                    parse_.offset = header_offset_;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        streams_[i].start_time = parse_.time_pcr.current();
                    }
                    archive_.seekg(header_offset_, std::ios_base::beg);
                    timestamp().max_delta(1000);
                    open_step_ = 4;
                }
            }

            if (ec) {
                return false;
            } else {
                assert(open_step_ == 4);
                on_open();
                return true;
            }
        }

        bool TsDemuxer::is_open(
            error_code & ec) const
        {
            if (open_step_ == 4) {
                ec = error_code();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        error_code TsDemuxer::close(
            error_code & ec)
        {
            if (open_step_ == 4) {
                on_close();
            }
            for (size_t i = 0; i < pes_parses_.size(); ++i) {
                streams_[i].clear();
                std::vector<ppbox::data::DataBlock> payloads;
                pes_parses_[i].clear(payloads);
            }
            streams_.clear();
            pes_parses_.clear();
            stream_map_.clear();
            parse_.offset = 0;
            parse_.had_pcr = false;
            parse2_.offset = 0;
            parse2_.had_pcr = false;
            header_offset_ = 0;
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t TsDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (is_open(ec)) {
                ec.clear();
                parse_.offset = header_offset_;
                parse_.time_pcr = streams_[0].start_time;
                parse2_ = parse_;
                for (size_t i = 0; i < pes_parses_.size(); ++i) {
                    std::vector<ppbox::data::DataBlock> payloads;
                    pes_parses_[i].clear(payloads);
                }
                dts.assign(dts.size(), streams_[0].start_time); // TO BE FIXED
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
            archive_.seekg(parse_.offset, std::ios::beg);
            assert(archive_);
            if (get_pes(ec)) {
                TsStream & stream = streams_[pes_index_];
                PesParse & parse = pes_parses_[pes_index_];
                BasicDemuxer::begin_sample(sample);
                sample.itrack = pes_index_;
                sample.flags = 0; // TODO: is_sync
                if (stream.type == StreamType::VIDE && parse.is_sync_frame(archive_)) {
                    sample.flags |= Sample::f_sync;
                }
                sample.dts = parse.dts();
                sample.cts_delta = parse.cts_delta();
                sample.duration = 0;
                sample.size = parse.size();
                sample.stream_info = &stream;
                free_pes(BasicDemuxer::datas());
                BasicDemuxer::end_sample(sample);
            }
            assert(archive_);
            return ec;
        }

        boost::uint32_t TsDemuxer::probe(
            boost::uint8_t const * hbytes, 
            size_t hsize)
        {
            boost::uint32_t scope = 0;
            for (size_t i = 0; i < hsize; i += TsPacket::PACKET_SIZE) {
                if (hbytes[i] == 0x47) {
                    ++scope;
                }
            }
            if (scope > SCOPE_MAX) {
                scope = SCOPE_MAX;
            }
            return scope;
        }

        boost::uint64_t TsDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t pcr = parse_.time_pcr.current();
            return timestamp().const_adjust(0, pcr);
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
            //assert(end > TsPacket::PACKET_SIZE);
            end = (end / TsPacket::PACKET_SIZE) * TsPacket::PACKET_SIZE;

            if (parse2_.offset < parse_.offset) {
                parse2_ = parse_;
            }

            if (parse2_.offset > end) { // 有可能外面数据清除了
                parse2_.offset = 0;
                parse2_.time_pcr = streams_[0].start_time;
            }

            if (parse2_.offset + TsPacket::PACKET_SIZE * 20 < end) {
                parse2_.offset = end - TsPacket::PACKET_SIZE * 20;
            }
            archive_.seekg(parse2_.offset, std::ios::beg);
            assert(archive_);
            while (parse2_.offset < end && get_packet(parse2_, ec)) {
                skip_packet(parse2_);
                if (parse2_.pkt.has_pcr()) {
                    break;
                }
            }
            archive_.seekg(beg, std::ios::beg);
            ec.clear();
            boost::uint64_t pcr = parse2_.time_pcr.current();
            return timestamp().const_adjust(0, pcr);
        }

        JointShareInfo * TsDemuxer::joint_share()
        {
            return new TsJointShareInfo(*this);
        }

        void TsDemuxer::joint_share(
            JointShareInfo * info)
        {
            if (open_step_ != 4) {
                TsJointShareInfo * ts_info = static_cast<TsJointShareInfo *>(info);
                streams_ = ts_info->streams_;
                stream_map_ = ts_info->stream_map_;
                pes_parses_.clear();
                for (size_t i = 0; i < streams_.size(); ++i) {
                    pes_parses_.push_back(PesParse(streams_[i].stream_type));
                }
                if (open_step_ < 3) {
                    open_step_ = 3;
                }
            }
        }

        void TsDemuxer::joint_begin(
            JointContext & context)
        {
            BasicDemuxer::joint_begin(context);
            if (jointer().read_ctx().data()) {
                TsJointData * data = static_cast<TsJointData *>(jointer().read_ctx().data());
                pes_parses_.swap(data->pes_parses_);
                context.read_ctx().data(NULL);
            }
        }

        void TsDemuxer::joint_end()
        {
            TsJointData * data = new TsJointData(*this);
            for (size_t i = 0; i < pes_parses_.size(); ++i) {
                if (streams_[i].type == StreamType::VIDE) {
                    data->pes_parses_[i].save_for_joint(archive_);
                }
            }
            jointer().read_ctx().data(data);
            BasicDemuxer::joint_end();
        }

        bool TsDemuxer::get_packet(
            TsParse & parse, 
            error_code & ec)
        {
            archive_.seekg(TsPacket::PACKET_SIZE, std::ios::cur);
            if (archive_) {
                archive_.seekg(-(int)TsPacket::PACKET_SIZE, std::ios::cur);
                assert(archive_);
                archive_ >> parse.pkt;
                if (archive_) {
                    if (parse.pkt.has_pcr()) {
                        parse.had_pcr = true;
                        parse.time_pcr.transfer(
                            ((boost::uint64_t)parse.pkt.adaptation.program_clock_reference_base) << 1);
                    }
                    ec.clear();
                    return true;
                } else if (archive_.failed()) {
                    archive_.clear();
                    ec = error::bad_file_format;
                } else {
                    archive_.clear();
                    ec = error::file_stream_error;
                }
            } else {
                archive_.clear();
                ec = error::file_stream_error;
            }
            return false;
        }

        void TsDemuxer::skip_packet(
            TsParse & parse)
        {
            parse.offset += TsPacket::PACKET_SIZE;
            archive_.seekg(parse.offset, std::ios::beg);
            assert(archive_);
        }

        bool TsDemuxer::get_pes(
            boost::system::error_code & ec)
        {
            while (get_packet(parse_, ec)) {
                if (parse_.pkt.pid >= stream_map_.size() || stream_map_[parse_.pkt.pid] == (size_t)-1) {
                    skip_packet(parse_);
                    continue;
                }
                pes_index_ = stream_map_[parse_.pkt.pid];
                std::pair<bool, bool> res = 
                    pes_parses_[pes_index_].add_packet(parse_.pkt, archive_, ec);
                if (res.second) {
                    parse_.offset -= TsPacket::PACKET_SIZE;
                }
                if (res.first) {
                    break;
                }
                skip_packet(parse_);
            }
            return !ec;
        }

        void TsDemuxer::free_pes()
        {
            std::vector<ppbox::data::DataBlock> payloads;
            free_pes(payloads);
        }

        void TsDemuxer::free_pes(
            std::vector<ppbox::data::DataBlock> & payloads)
        {
            PesParse & parse = pes_parses_[pes_index_];
            pes_index_ = size_t(-1);
            parse.clear(payloads);
            skip_packet(parse_);
        }

    }
}

// Live2Source.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/live2/Live2Source.h"
#include "ppbox/demux/pptv/PptvJump.h"
#include "ppbox/demux/base/DemuxerError.h"

#include <util/protocol/pptv/Url.h>
#include <util/protocol/pptv/TimeKey.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h> 

#include <framework/string/Format.h>
#include <framework/timer/Timer.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace boost::system;
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live2Source", 0);

#ifndef PPBOX_DNS_LIVE2_JUMP
#  define PPBOX_DNS_LIVE2_JUMP "(tcp)(v4)live.dt.synacast.com:80"
#endif

#define P2P_HEAD_LENGTH 1400

namespace ppbox
{
    namespace demux
    {
        static const  framework::network::NetName dns_live2_jump_server(PPBOX_DNS_LIVE2_JUMP);

        static const boost::uint32_t CACHE_T = 1800;

        static inline std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        Live2Source::Live2Source(
            boost::asio::io_service & io_svc)
            : HttpSource(io_svc)
             , jump_(new PptvJump(io_svc, JumpType::live))
             , open_step_(StepType::not_open)
             , time_(0)
             , live_port_(0)
             , server_time_(0)
             , file_time_(0)
             , begin_time_(0)
             , value_time_(0)
             , seek_time_(0)
             , bwtype_(2)
             , index_(0)
             , interval_(5)
        {
        }

        Live2Source::~Live2Source()
        {
            if (jump_)
            {
                delete jump_;
                jump_ = NULL;
            }
        }

        void Live2Source::async_open(SourceBase::response_type const &resp)
        {
            resp_ = resp;
            open_step_ = StepType::opening;
            handle_async_open(boost::system::error_code());
        }

        framework::string::Url  Live2Source::get_jump_url() const
        {
            framework::string::Url url("http://localhost/");
            url.host(dns_live2_jump_server.host());
            url.svc(dns_live2_jump_server.svc());
            url.path("/live2/" + stream_id_);
            return url;
        }

        void Live2Source::parse_jump(
            Live2JumpInfo & jump_info, 
            boost::asio::streambuf & buf, 
            boost::system::error_code & ec)
        {
            if (!ec) {
                std::string buffer = boost::asio::buffer_cast<char const *>(buf.data());
                LOG_S(Logger::kLevelDebug2, "[parse_jump] jump buffer: " << buffer);

                util::archive::XmlIArchive<> ia(buf);
                ia >> jump_info;
                if (!ia) {
                    ec = error::bad_file_format;
                }
            }
        }

        std::string Live2Source::get_key() const
        {
            return util::protocol::pptv::gen_key_from_time(server_time_ + (Time::now() - local_time_).total_seconds());
        }

        void Live2Source::set_info_by_jump(
            Live2JumpInfo & jump_info)
        {
            jump_info_ = jump_info;
            local_time_ = Time::now();
            server_time_ = jump_info_.server_time.to_time_t();
            server_time_ = server_time_ / interval_ * interval_;
            file_time_ = server_time_;
            begin_time_ = server_time_ - CACHE_T;
            value_time_ = CACHE_T - jump_info_.delay_play_time;
            max_segment_size_ = CACHE_T / interval_;
            tc_.reset();
        }

        boost::system::error_code Live2Source::reset(
            SegmentPositionEx & segment)
        {
            segment.segment = (value_time_+(tc_.elapsed()/1000))/interval_ - 1;
            file_time_ = begin_time_ + (segment.segment * interval_);
            segment.source = this;
            segments_.clear();
            add_segment(segment);
            begin_segment_ = segment;
            return error_code();
        }

        void Live2Source::handle_async_open(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (ec) {
                if (ec != boost::asio::error::would_block) {
                    if (open_step_ == StepType::jump) {
                        LOG_S(Logger::kLevelAlarm, "jump: failure");
                        response(ec);
                    }
                }
                return;
            }

            switch (open_step_) 
            {
            case StepType::opening:
                {
                    if (!ec) 
                    {
                        open_step_ = StepType::jump;

                        LOG_S(Logger::kLevelEvent, "jump: start");

                        jump_->async_get(
                            get_jump_url(), 
                            boost::bind(&Live2Source::handle_async_open, this, _1));
                        return;
                    }
                    else 
                    {
                        LOG_S(Logger::kLevelDebug, "url ec: " << ec.message());
                    }
                    break;
                }
            case StepType::jump:
                {
                    Live2JumpInfo jump_info;
                    parse_jump(jump_info, jump_->get_buf(), ec);
                    if (!ec)
                    {
                        set_info_by_jump(jump_info);
                    }
                    open_step_ = StepType::finish;
                    break;
                }
            default:
                assert(0);
                return;
            }

            response(ec);
        }

        void Live2Source::response(
            boost::system::error_code const & ec)
        {
            SourceBase::response_type resp;
            resp.swap(resp_);
            ios_service().post(boost::bind(resp, ec));
        }


        bool Live2Source::is_open()
        {
            return (open_step_ > StepType::jump);
        }

        void Live2Source::update_segment(size_t segment)
        {
            if (segments_.size() == segment ) {
                /*VodSegmentNew newSegment;
                newSegment.duration = boost::uint32_t(-1);
                newSegment.file_length = boost::uint64_t(-1);
                newSegment.head_length = boost::uint64_t(-1);
                segments_.push_back(newSegment);*/
            } else if (segments_.size() > segment) {
                //正常情况
            } else {
                //assert(false);
            }
        }

        void Live2Source::update_segment_duration(
            size_t segment,
            boost::uint32_t time)
        {
            bool find = false;
            for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                if (segments_[i].segment == segment) {
                    find = true;
                    segments_[i].time_end = time;
                }
            }
            assert(find);
        }

        void Live2Source::update_segment_file_size(
            size_t segment,
            boost::uint64_t filesize)
        {
            boost::uint32_t find_this_segment = boost::uint32_t(-1);
            boost::uint32_t find_next_segment = boost::uint32_t(-1);
            for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                if (segments_[i].segment == segment) {
                    find_this_segment = i;
                    if (0 == i) {
                        assert(segments_[i].total_state != SegmentPositionEx::is_valid);
                        // 只有第一段才进入这里
                        segments_[i].size_beg = segments_[i].shard_beg = 0;
                        segments_[i].size_end = filesize;
                        segments_[i].shard_end = segments_[i].size_end;
                    } else {
                        assert(segments_[i-1].total_state == SegmentPositionEx::is_valid);
                        assert(segments_[i].total_state < SegmentPositionEx::is_valid);
                        segments_[i].size_beg = segments_[i].shard_beg = segments_[i-1].size_end;
                        segments_[i].size_end = segments_[i].size_beg + filesize;
                        segments_[i].shard_end = segments_[i].size_end;
                        assert(segments_[i].size_beg != segments_[i].size_end);
                    }
                    if (segments_[i].shard_end != (boost::uint64_t)-1) {
                        segments_[i].total_state = SegmentPositionEx::is_valid;
                    } else {
                        segments_[i].total_state = SegmentPositionEx::not_exist;
                    }
                }
                if (segments_[i].segment == (segment + 1)) {
                    find_next_segment = i;
                }
            }
            assert(find_this_segment != boost::uint32_t(-1));
            assert(find_this_segment != find_next_segment);

            if (find_this_segment != boost::uint32_t(-1) 
                && find_next_segment == boost::uint32_t(-1)) {
                SegmentPositionEx seg;
                seg.segment = segment + 1;
                add_segment(seg);
            }
        }

        boost::system::error_code Live2Source::segment_open(
            size_t segment, 
            boost::uint64_t beg, 
            boost::uint64_t end, 
            boost::system::error_code & ec)
        {
            update_segment(segment);
            HttpSource::segment_open(segment,beg,end,ec);
            return ec;
        }

        void Live2Source::on_error(
            boost::system::error_code & ec)
        {
            if (ec.value() == 404) {
                ec.clear();
                buffer()->pause(5000);
            }
        }

        void Live2Source::segment_async_open(
            size_t segment, 
            boost::uint64_t beg, 
            boost::uint64_t end, 
            SourceBase::response_type const & resp) 
        {
            update_segment(segment);
            HttpSource::segment_async_open(segment,beg,end,resp);
        }

        boost::system::error_code Live2Source::get_request(
            size_t segment, 
            boost::uint64_t & beg, 
            boost::uint64_t & end, 
            framework::network::NetName & addr, 
            util::protocol::HttpRequest & request, 
            boost::system::error_code & ec)
        {
            util::protocol::HttpRequestHead & head = request.head();
            ec = boost::system::error_code();

            if (segment == 0) {
                if (live_port_){
                    set_time_out(0, ec);
                    addr.host("127.0.0.1");
                    addr.port(live_port_);
                    std::string url("http://");
                    url += jump_info_.server_host.host();
                    url += ":";
                    url += jump_info_.server_host.svc();
                    url += "/live/";
                    url = framework::string::Url::encode(url);
                    std::string patcher("/playlive.flv?url=");
                    patcher += url;
                    patcher += "&channelid=";
                    patcher += jump_info_.channelGUID;
                    patcher += framework::string::join(rid_.begin(), rid_.end(), "@", "&rid=");
                    patcher += framework::string::join(rate_.begin(), rate_.end(), "@", "&datarate=");
                    patcher += "&replay=1";
                    patcher += "&start=";
                    patcher += framework::string::format(file_time_);
                    patcher += "&interval=";
                    patcher += framework::string::format(interval_);
                    patcher += "&BWType=";
                    patcher += framework::string::format(bwtype_);
                    patcher += "&source=0&uniqueid=";
                    patcher += framework::string::format(seq_);
                    head.path = patcher;
                } else {
                    set_time_out(5 * 1000, ec);
                    addr = jump_info_.server_host;
                    /*if (!proxy_addr_.host().empty()) {
                        addr = proxy_addr_;
                    }*/
                }
                head.host.reset(addr_host(addr));
                head.connection = util::protocol::http_field::Connection::keep_alive;
            }

            if (live_port_) {
            } else {
                beg += P2P_HEAD_LENGTH;
                if (end != (boost::uint64_t)-1) {
                    end += P2P_HEAD_LENGTH;
                }
                head.path = "/live/" + stream_id_ + "/" + format(file_time_) + ".block";
                addr = jump_info_.server_host;
                //head.host.reset(addr_host(jump_info_.server_host));
            }

            return ec;
        }

        void Live2Source::add_segment(SegmentPositionEx & segment)
        {
            assert(segments_.size() <= max_segment_size_);
            if (segments_.size() == max_segment_size_) {
                segments_.pop_front();
            }
            bool find = false;
            for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                if (segment.segment == segments_[i].segment) {
                    find = true;
                }
            }
            if (!find) {
                if (!segments_.empty()) {
                     assert(segment.segment == (segments_[segments_.size()-1].segment + 1));
                     segment.size_beg = segment.shard_beg = segments_[segments_.size()-1].size_end;
                }
                segment.time_state = SegmentPositionEx::by_guess;
                segment.time_beg = segment.segment * interval_ * 1000;
                segment.time_end = segment.time_beg + (interval_ * 1000);
                segments_.push_back(segment);
            }
        }

        DemuxerType::Enum Live2Source::demuxer_type() const
        {
            return DemuxerType::flv;
        }

        void Live2Source::set_url(std::string const &url)
        {
            std::string::size_type slash = url.find('|');
            if (slash == std::string::npos) {
                return;
            }

            key_ = url.substr(0, slash);
            std::string playlink = url.substr(slash + 1);

            playlink = framework::string::Url::decode(playlink);
            framework::string::Url request_url(playlink);
            playlink = request_url.path().substr(1);

            std::string strSeek = request_url.param("seek");
            if(!strSeek.empty())
            {
                seek_time_ = framework::string::parse<boost::uint32_t>(strSeek);
            }
            std::string strBWType = request_url.param("bwtype");
            if(!strBWType.empty())
            {
                bwtype_ = framework::string::parse<boost::int32_t>(strBWType);
            }

            if (playlink.find('-') != std::string::npos) {
                // "[StreamID]-[Interval]-[datareate]
                url_ = playlink;
                std::vector<std::string> strs;
                slice<std::string>(playlink, std::inserter(strs, strs.end()), "-");
                if (strs.size() >= 3) {
                    name_ = "Stream:" + strs[0];
                    rid_.push_back(strs[0]);
                    stream_id_ = rid_[0];
                    parse2(strs[1], interval_);
                    boost::uint32_t rate = 0;
                    parse2(strs[2], rate);
                    rate_.push_back(rate);
                } else {
                    std::cout<<"Wrong URL Param"<<std::endl;
                }
                return;
            }
            url_ = util::protocol::pptv::base64_decode(playlink, key_);
            if (!url_.empty()) {
                map_find(url_, "name", name_, "&");
                map_find(url_, "channel", channel_, "&");
                map_find(url_, "interval", interval_, "&");
                std::string sid, datarate;
                map_find(url_, "sid", sid, "&");
                slice<std::string>(sid, std::inserter(rid_, rid_.end()), "@");
                if (!rid_.empty())
                    stream_id_ = rid_[0];
                map_find(url_, "datarate", datarate, "&");
                slice<boost::uint32_t>(datarate, std::inserter(rate_, rate_.end()), "@");
            }
        }

        boost::system::error_code Live2Source::get_duration(
            DurationInfo & info,
            boost::system::error_code & ec)
        {
            ec.clear();
            info.total = 0;
            if (live_port_) {
                info.begin = 0;
                info.end = 0;
                info.redundancy = 0;
                info.interval = 0;
            } else {
                info.total = 0;
                info.begin = begin_time_ + (tc_.elapsed() / 1000);
                info.end = info.begin + value_time_;
                info.redundancy = jump_info_.delay_play_time;
                info.interval = interval_;
            }
           return ec;
        }

        boost::system::error_code Live2Source::time_seek (
            boost::uint64_t time, // 毫妙
            SegmentPositionEx & abs_position,
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (live_port_) {
                ec = error::not_support;
            } else {
                boost::uint64_t iTime = 0;
                file_time_ = time/1000 + begin_time_;
                file_time_ = file_time_ / interval_ * interval_;
                position.segment = time / (interval_ * 1000);
                position.time_beg = position.segment * interval_ * 1000;
                position.time_end = position.time_beg + (interval_ * 1000);
                position.time_state = SegmentPositionEx::by_guess;

                bool find = false;
                for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                    if (segments_[i].segment == position.segment) {
                        position.source = this;
                        position.total_state = segments_[i].total_state;
                        position.size_beg = segments_[i].size_beg;
                        position.size_end = segments_[i].size_end;
                        position.time_beg = segments_[i].time_beg;
                        position.time_end = segments_[i].time_end;
                        position.shard_beg = segments_[i].shard_beg;
                        position.shard_end = segments_[i].shard_end;
                        find = true;
                        break;
                    }
                }
                if (!find || position.total_state < SegmentPositionEx::is_valid) {
                    segments_.clear();
                    begin_segment_ = position;
                    abs_position = position;
                    add_segment(position);
                }

            }
            return ec;
        }

        bool Live2Source::next_segment(
            SegmentPositionEx & segment)
        {
            if (live_port_) {
                return false;
            } else {
                if (!segment.source) {
                    segment.segment = 0;
                    segment.source = this;
                    segment.shard_beg = segment.size_beg = segment.size_beg;
                    boost::uint64_t seg_size = segment_size(segment.segment);
                    segment.shard_end = segment.size_end = (seg_size == (boost::uint64_t)-1 ? -1: segment.size_beg + seg_size);
                    segment.time_beg = segment.time_beg;
                    boost::uint64_t seg_time = segment_time(segment.segment);
                    segment.time_end = 
                        ( seg_time == (boost::uint64_t)-1 ? (boost::uint64_t)-1: segment.time_beg + segment_time(segment.segment));
                } else {
                    segment.segment++;
                    file_time_ = begin_time_ + (segment.segment * interval_);
                    segment.size_beg = segment.size_end;
                    boost::uint64_t seg_size = segment_size(segment.segment);
                    segment.size_end = 
                        (seg_size == (boost::uint64_t)-1 ? (boost::uint64_t)-1 : segment.size_beg + segment_size(segment.segment));
                    segment.shard_beg = segment.size_beg;
                    segment.shard_end = segment.size_end;
                    segment.time_beg = segment.time_end;
                    boost::uint64_t seg_time = segment_time(segment.segment);
                    segment.time_end = 
                        (seg_time == (boost::uint64_t)-1 ? (boost::uint64_t)-1 : segment.time_beg + segment_time(segment.segment));
                }
                if (segment.size_end != (boost::uint64_t)-1) {
                    segment.total_state = SegmentPositionEx::is_valid;
                } else {
                    segment.total_state = SegmentPositionEx::not_exist;
                }
                return true;
            }
        }

        size_t Live2Source::segment_count() const
        {
            size_t ret = size_t(-1);
            return ret;
        }

        boost::uint64_t Live2Source::segment_size(size_t segment)
        {
            boost::uint64_t ret = (boost::uint64_t)-1;
            for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                if (segments_[i].segment == segment) {
                    ret = segments_[i].size_end - segments_[i].size_beg;
                    break;
                }
            }
            return ret;
        }

        boost::uint64_t Live2Source::segment_time(size_t segment)
        {
            boost::uint64_t ret = boost::uint64_t(-1);
            for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
                if (segments_[i].segment == segment) {
                    ret = segments_[i].time_end - segments_[i].time_beg;
                    break;
                }
            }
            return ret;
        }

    } // namespace demux
} // namespace ppbox

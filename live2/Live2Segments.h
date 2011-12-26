// Live2Segments.h

#ifndef _PPBOX_DEMUX_LIVE2_SEGMENTS_H_
#define _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/live2/Live2Demuxer.h"
#include "ppbox/common/Serialize.h"

#include <util/protocol/pptv/Base64.h>
#include <util/serialization/NVPair.h>
#include <util/serialization/stl/vector.h>
using namespace util::protocol;

#include <framework/string/Slice.h>
#include <framework/string/Url.h>
#include <framework/timer/ClockTime.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::string;
using namespace framework::network;
using namespace framework::system::logic_error;

using namespace boost::system;

#define P2P_HEAD_LENGTH     1400

namespace ppbox
{
    namespace demux
    {
#ifdef API_PPLIVE
        static const NetName dns_live2_jump_server("(tcp)(v4)live.dt.synacast.com:80");
#else
        static const NetName dns_live2_jump_server("(tcp)(v4)live.dt.synacast.com:80");
#endif

        struct Live2JumpInfo
        {
            NetName server_host;
            std::vector<NetName> server_hosts;
            util::serialization::UtcTime server_time;
            boost::uint16_t delay_play_time;
            std::string channelGUID;
            std::string server_limit;
            std::vector<std::string> server_limits;

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(server_host)
                    & SERIALIZATION_NVP(server_time)
                    & SERIALIZATION_NVP(delay_play_time)
                    & SERIALIZATION_NVP(server_limit)
                    & SERIALIZATION_NVP(channelGUID)
                    & SERIALIZATION_NVP(server_hosts)
                    & SERIALIZATION_NVP(server_limits);
            }
        };

        struct Live2Stream
        {
            boost::uint16_t delay_play_time;

            template<
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & util::serialization::make_nvp("delay", delay_play_time);
            }
        };

        struct Live2Channel
        {
            std::string channelGUID;
            Live2Stream stream;

            template<
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & util::serialization::make_nvp("rid", channelGUID)
                    & SERIALIZATION_NVP(stream);
            }
        };

        struct Live2sh
        {
            std::string server_limit;

            template<
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & util::serialization::make_nvp("limit", server_limit);
            }
        };

        struct Live2dt
        {
            NetName server_host;
            Live2sh sh;
            util::serialization::UtcTime server_time;

            template<
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & util::serialization::make_nvp("sh", server_host)
                    & SERIALIZATION_NVP(sh)
                    & util::serialization::make_nvp("st", server_time);
            }
        };

        struct Live2JumpInfoNew
        {
            Live2Channel channel;
            Live2dt dt;

            template<
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & SERIALIZATION_NVP(channel)
                    & SERIALIZATION_NVP(dt);
            }
        };

        static std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        class Live2Segments
            : public HttpSource
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live2Segments", 0);

        public:
            Live2Segments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port)
                : HttpSource(io_svc)
                , live_port_(live_port)
                , num_del_(0)
                , server_time_(0)
                , file_time_(0)
                , index_(0)
                , interval_(10)
                , live_demuxer_(NULL)
            {
            }

        public:
            virtual error_code get_request(
                size_t segment, 
                boost::uint64_t & beg, 
                boost::uint64_t & end, 
                framework::network::NetName & addr, 
                util::protocol::HttpRequest & request, 
                error_code & ec)
            {
                beg += P2P_HEAD_LENGTH;
                if (end != (boost::uint64_t)-1) {
                    end += P2P_HEAD_LENGTH;
                }

                util::protocol::HttpRequestHead & head = request.head();
                ec = error_code();
                if (segment == 0) {
                    set_time_out(5 * 1000, ec);
                    if (!proxy_addr_.host().empty()) {
                        addr = proxy_addr_;
                    } else if (!live_port_){
                        addr = jump_info_.server_host;
                    } else {
                        addr.host("127.0.0.1");
                        addr.port(live_port_);
                    }
                    head.host.reset(addr_host(addr));
                    head.connection = util::protocol::http_field::Connection::keep_alive;
                }

                head.path = "/live/" + stream_id_ + "/" + format(file_time_) + ".block";

                return ec;
            }

            virtual DemuxerType::Enum demuxer_type() const
            {
                return DemuxerType::flv;
            }

            virtual void on_seg_beg(
                size_t segment)
            {
                live_demuxer_->seg_beg(segment);
            }

            virtual void on_seg_close(
                size_t segment)
            {
                live_demuxer_->seg_end(segment);

                LOG_S(framework::logger::Logger::kLevelDebug, 
                    "[on_seg_close] segment: " << segment << ", file_time_: " << file_time_);
            }

            virtual void on_error(
                boost::system::error_code & ec)
            {
                LOG_S(framework::logger::Logger::kLevelDebug, 
                    "[on_error] ec: " << ec.message());

                if (ec == source_error::no_more_segment) {
                    update();
                    ec.clear();
                } else if (ec == http_error::not_found) {
                    buffer()->pause(5 * 1000);
                    ec.clear();
                    //ec = boost::asio::error::would_block;
                }
            }

        public:
            void set_url_key(
                std::string const & key, 
                std::string const & url)
            {
                key_ = key;

                //std::string tmp = pptv::base64_encode("channel={831db0b0-08a2-4e4d-9dc1-739cbab9afe3}&name=live2 test&sid=e9301e073cf94732a380b765c8b9573d@0a5adf8f23494bbf970e3ce02b1b73a2@9cd54184428347f7bd663efa39fc4891&datarate=51200@68900@71200&interval=5", key);
                //std::string tmp = "e9301e073cf94732a380b765c8b9573d-5";

                if (url.find('-') != std::string::npos) {
                    // "[StreamID]-[Interval]
                    url_ = url;
                    std::vector<std::string> strs;
                    slice<std::string>(url, std::inserter(strs, strs.end()), "-");
                    if (strs.size() == 2) {
                        name_ = "Stream:" + strs[0];
                        rid_.push_back(strs[0]);
                        stream_id_ = rid_[0];
                        parse2(strs[1], interval_);
                    }
                    return;
                }

                url_ = pptv::base64_decode(url, key);
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

            framework::string::Url get_jump_url()
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_live2_jump_server.host());
                url.svc(dns_live2_jump_server.svc());
                url.path("/live2/" + stream_id_);

                //framework::string::Url url("http://web-play.pptv.com/webplay3-0-300147.xml&param=type=web&userType=0&areaType=1&dns=12345?r=1319010213406");

                return url;
            }

            void set_jump_info(
                Live2JumpInfo const & jump_info)
            {
                jump_info_ = jump_info;

                local_time_ = Time::now();

                server_time_ = jump_info_.server_time.to_time_t();
                file_time_ = server_time_ - jump_info_.delay_play_time;

                file_time_ = file_time_ / interval_ * interval_;
            }

            void update()
            {
                // 当前分段已经下载的时间
                file_time_ += interval_;
                // 当前已经播放时长
                boost::int64_t total_seconds = (Time::now() - local_time_).total_seconds();
                if (total_seconds + server_time_ > file_time_ + jump_info_.delay_play_time * 2) {
                    // 跳段时，保证比正常播放时间延迟 delay_play_time
                    while (total_seconds + server_time_ > file_time_ + jump_info_.delay_play_time) {
                        file_time_ += interval_;
                    }
                }
            }

            void set_http_proxy(
                framework::network::NetName const & addr)
            {
                proxy_addr_ = addr;
            }

            void set_live_demuxer(
                Live2Demuxer * live_demuxer)
            {
                live_demuxer_ = live_demuxer;
            }

            std::string const & get_name() const
            {
                return name_;
            }

            std::string const & get_uuid() const
            {
                return channel_;
            }

            boost::uint16_t interval() const
            {
                return interval_;
            }

            DemuxerType::Enum demuxer_type()
            {
                return DemuxerType::flv;
            }

        private:
            size_t segment_count() const
            {
                return (size_t)-1;
            }

            boost::uint64_t segment_size(
                size_t segment)
            {
                if (segment < segments_.size() + num_del_) {
                    return segments_[segment - num_del_];
                }
                return boost::uint64_t(-1);
            }

            boost::uint64_t segment_time(
                size_t segment)
            {
                return boost::uint64_t(-1);
            }

            void next_segment(
                SegmentPositionEx & segment)
            {
                if (!segment.source) {
                    segment.source = this;
                    segment.shard_beg = segment.size_beg = segment.size_beg;
                    segment.shard_end = segment.size_end = boost::uint64_t(-1);
                    segment.time_beg = segment.time_beg;
                    segment.time_end = boost::uint64_t(-1);
                } else {
                    if (segment.segment - num_del_ >= segments_.size()) {
                        segments_.push_back(segment.size_end - segment.size_beg);
                        update();
                    }
                    ++segment.segment;
                    segment.shard_beg = segment.size_beg = segment.size_end;
                    segment.shard_end = segment.size_end = 
                        segment_size(segment.segment) == boost::uint64_t(-1) ? 
                        boost::uint64_t(-1) : segment_size(segment.segment) + segment.size_beg;
                    if (segment.size_end == (boost::uint64_t)-1) {
                        segment.total_state = SegmentPositionEx::not_init;
                    } else {
                        segment.total_state = SegmentPositionEx::is_valid;
                    }
                    segment.time_beg = segment.time_end;
                    segment.time_end = boost::uint64_t(-1);
                }
                while (num_del_ < buffer()->read_segment().segment) {
                    num_del_++;
                    segments_.pop_front();
                }
            }

        private:
            boost::uint16_t live_port_;
            boost::uint32_t num_del_;
            framework::network::NetName proxy_addr_;

        private:
            std::string key_;
            std::string url_;
            std::string name_;
            std::string channel_;
            std::string stream_id_;

            Live2JumpInfo jump_info_;
            Time local_time_;
            time_t server_time_;
            time_t file_time_;

            int index_;
            std::vector<std::string> rid_;
            std::vector<boost::uint32_t> rate_;

            std::deque<boost::uint64_t> segments_;

            boost::uint16_t interval_;

            Live2Demuxer * live_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

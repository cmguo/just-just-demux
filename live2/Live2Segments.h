// Live2Segments.h

#ifndef _PPBOX_DEMUX_LIVE2_SEGMENTS_H_
#define _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/live2/Live2Demuxer.h"

#include <ppbox/common/Serialize.h>
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

#ifndef PPBOX_DNS_LIVE2_JUMP
#  define PPBOX_DNS_LIVE2_JUMP "(tcp)(v4)live.dt.synacast.com:80"
#endif

namespace ppbox
{
    namespace demux
    {

        static const NetName dns_live2_jump_server(PPBOX_DNS_LIVE2_JUMP);

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
                , seek_time_(0)
                , bwtype_(0)
                , index_(0)
                , interval_(10)
                , live_demuxer_(NULL)
            {
                static boost::uint32_t g_seq = 1;
                seq_ = g_seq++;
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
                util::protocol::HttpRequestHead & head = request.head();
                ec = error_code();
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
                        if (!proxy_addr_.host().empty()) {
                            addr = proxy_addr_;
                        }
                    }
                    head.host.reset(addr_host(addr));
                    head.connection = util::protocol::http_field::Connection::keep_alive;
                }

                if (live_port_)
                {
                }
                else
                {
                    beg += P2P_HEAD_LENGTH;
                    if (end != (boost::uint64_t)-1) {
                        end += P2P_HEAD_LENGTH;
                    }
                    head.path = "/live/" + stream_id_ + "/" + format(file_time_) + ".block";
                }

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
            void set_name(std::string const & name)
            {
                std::string::size_type slash = name.find('|');
                if (slash == std::string::npos) 
                {
                    return;
                }

                key_ = name.substr(0, slash);
                std::string url = name.substr(slash + 1);
                //std::string tmp = "e9301e073cf94732a380b765c8b9573d-5";

                //解URL
                //std::string   temp_host = "http://host/";
                //url = temp_host + url;
                url = framework::string::Url::decode(url);
                framework::string::Url request_url(url);
                url = request_url.path().substr(1);

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

                if (url.find('-') != std::string::npos) {
                    // "[StreamID]-[Interval]-[datareate]
                    url_ = url;
                    std::vector<std::string> strs;
                    slice<std::string>(url, std::inserter(strs, strs.end()), "-");
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
                url_ = pptv::base64_decode(url, key_);
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
                if (seek_time_ > 0)
                {
                    std::cout<<"Live2 seek_time_:"<<seek_time_<<std::endl;
                    file_time_ = server_time_ - jump_info_.delay_play_time-seek_time_;
                }
                else
                {
                    file_time_ = server_time_ - jump_info_.delay_play_time;
                }


                file_time_ = file_time_ / interval_ * interval_;

			}

            void set_file_time(boost::uint32_t iTime,bool bSign)  //sign ture +++  false ---
            {
                file_time_ = server_time_ - jump_info_.delay_play_time;

                if(bSign)
                {
                    file_time_+=iTime/1000;
                }
                else
                {
                    file_time_-=iTime/1000;
                }
                file_time_ = file_time_ / interval_ * interval_;
	
            }

            void update()
            {
                // 当前分段已经下载的时间
                file_time_ += interval_;
                // 当前已经播放时长
                /*boost::int64_t total_seconds = (Time::now() - local_time_).total_seconds();
                if (total_seconds + server_time_ > file_time_ + jump_info_.delay_play_time * 2) {
                    // 跳段时，保证比正常播放时间延迟 delay_play_time
                    while (total_seconds + server_time_ > file_time_ + jump_info_.delay_play_time) {
                        LOG_S(framework::logger::Logger::kLevelAlarm, 
                            "[next_segment] skip " << interval_ << "seconds");
                        file_time_ += interval_;
                    }
                }*/
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
            boost::uint32_t seek_time_;
            boost::int32_t bwtype_;

            int index_;
            std::vector<std::string> rid_;
            std::vector<boost::uint32_t> rate_;

            std::deque<boost::uint64_t> segments_;

            boost::uint16_t interval_;

            Live2Demuxer * live_demuxer_;

            boost::uint32_t seq_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

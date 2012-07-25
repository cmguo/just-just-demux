// Live3Segments.h

#ifndef _PPBOX_DEMUX_LIVE2_PLAY_SEGMENTS_H_
#define _PPBOX_DEMUX_LIVE2_PLAY_SEGMENTS_H_

#include "ppbox/demux/source/HttpBufferList.h"
#include "ppbox/demux/Live3Demuxer.h"

#include <ppbox/common/DomainName.h>
#include <ppbox/common/DynamicString.h>
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

#ifndef PPBOX_DNS_LIVE2_PLAY
#define PPBOX_DNS_LIVE2_PLAY "(tcp)(v4)epg.api.pptv.com:80"
#endif

#ifndef JUMP_TYPE
#define JUMP_TYPE "ppbox"
#endif

#ifndef PPBOX_LIVE_PLATFORM
#define PPBOX_LIVE_PLATFORM "ppbox"
#endif

#ifndef PPBOX_LIVE_TYPE
#define PPBOX_LIVE_TYPE "ppbox"
#endif

namespace ppbox
{
    namespace demux
    {
        DEFINE_DOMAIN_NAME(dns_live2_play_server,PPBOX_DNS_LIVE2_PLAY);
        DEFINE_DYNAMIC_STRING(dns_live2_platform,PPBOX_LIVE_PLATFORM);
        DEFINE_DYNAMIC_STRING(dns_live2_type,PPBOX_LIVE_TYPE);

        struct LiveVideo
        {
            LiveVideo()
                : bitrate(0)
                , width(0)
                , height(0)
                , ft(-1)
            {}
            std::string rid;
            boost::uint32_t bitrate;    // 平均码流率
            boost::uint32_t width;
            boost::uint32_t height;
            boost::uint32_t ft;
            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(rid)
                    & SERIALIZATION_NVP(bitrate)
                    & SERIALIZATION_NVP(width)
                    & SERIALIZATION_NVP(height)
                    & SERIALIZATION_NVP(ft);
            }
        };

        struct LiveStream
        {
            LiveStream()
                : delay(0)
                , interval(0)
                , jump(0)
                , ft(-1)
            {
            }
            boost::uint32_t delay;
            boost::uint32_t interval;
            boost::uint32_t jump;
            LiveVideo video;
            boost::uint32_t ft;
            std::vector<LiveVideo> videos;
            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(delay)
                    & SERIALIZATION_NVP(interval)
                    & SERIALIZATION_NVP(jump)
                    & videos;

                this->finish();

            }
        private:
            void finish()
            {
                if(ft == (-1) && !find_ft())
                {
                    return;
                }
            }

            bool find_ft()
            {
                if(videos.size() < 1) return false;
                this->ft = videos[0].ft;
                video = videos[0];
                std::vector<LiveVideo>::iterator iter = videos.begin();
                for (; iter != videos.end(); ++iter)
                {
                    if (iter->ft > this->ft)
                    {
                        this->ft = iter->ft;
                        video = *iter;
                    }
                }
                return true;
            }
        };

        struct Channel
        {
            Channel()
                : id(0)
                , vt(0)
            {}
            boost::uint32_t id;
            boost::uint32_t vt;
            LiveStream stream;
            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(id)
                    & SERIALIZATION_NVP(vt)
                    & SERIALIZATION_NVP(stream);
            }
        };

        struct LiveDtInfoNew
        {
            LiveDtInfoNew()
                : bwt(-1)
                , st_t(0)
            {

            }
            framework::network::NetName sh;
            util::serialization::UtcTime st;
            boost::uint32_t bwt;
            time_t st_t;

            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(sh)
                    & SERIALIZATION_NVP(st)
                    & SERIALIZATION_NVP(bwt);
                st_t = st.to_time_t();
            }
        };

        struct LiveDtInfo
        {
            LiveDtInfo()
                : bwt(-1)
                , st_t(0)
            {

            }
            
            LiveDtInfo & operator=(
                LiveDtInfoNew const & r)
            {
                sh = r.sh;
                st = r.st;
                bwt = r.bwt;
                st_t = r.st_t;
                return *this;
            }

            framework::network::NetName sh;
            util::serialization::UtcTime st;
            boost::uint32_t bwt;
            framework::network::NetName bh;
            time_t st_t;

            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(sh)
                    & SERIALIZATION_NVP(st)
                    & SERIALIZATION_NVP(bwt)
                    & SERIALIZATION_NVP(bh);
                st_t = st.to_time_t();
            }
        }; 

        struct Live2PlayInfoNew
        {
            Channel channel;
            LiveDtInfoNew dt;
            framework::network::NetName uh;

            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(channel)
                    & SERIALIZATION_NVP(dt)
                    & SERIALIZATION_NVP(uh);
            }
        };

        struct Live2PlayInfo
        {
            Live2PlayInfo & operator=(
                Live2PlayInfoNew const & r)
            {
                channel = r.channel;
                dt = r.dt;
                uh = r.uh;
                return *this;
            }

            Channel channel;
            LiveDtInfo dt;
            framework::network::NetName uh;

            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar  & SERIALIZATION_NVP(channel)
                    & SERIALIZATION_NVP(dt)
                    & SERIALIZATION_NVP(uh);
            }
        };

        

        static std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        class Live3Segments
            : public HttpBufferList<Live3Segments>
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live3Segments", 0);

        public:
            Live3Segments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : HttpBufferList<Live3Segments>(io_svc, buffer_size, prepare_size)
                , live_port_(live_port)
                , server_time_(0)
                , file_time_(0)
                , seek_time_(0)
                , bwtype_(-1)
                , index_(0)
                , interval_(10)
                , live_demuxer_(NULL)
            {
                static boost::uint32_t g_seq = 1;
                seq_ = g_seq++;
            }

        public:
            error_code get_request(
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
                        //url += play_info_.server_host.host();
                        url += play_info_.dt.sh.host();
                        url += ":";
                        url += play_info_.dt.sh.svc();
                        url += "/live/";
                        url += "?type=";
                        url += type_;
                        url += "&vvid=";
                        url += vvid_;
                        url += "&platform=";
                        url += platform_;

                        LOG_S(framework::logger::Logger::kLevelDebug, "Segment url: " << url);
                        url = framework::string::Url::encode(url);

                        std::string patcher("/playlive.flv?url=");
                        patcher += url;
                        patcher += "&channelid=";
                        patcher += play_info_.channel.stream.video.rid;
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
                        LOG_S(framework::logger::Logger::kLevelDebug, "Use worker, BWType: " << bwtype_ 
                            << " start: " << file_time_ << " interval: " << interval_ << " seq_: " << seq_);
                    } else {
                        set_time_out(5 * 1000, ec);
                        addr = play_info_.dt.sh;
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
                    head.path = "/live/" + stream_id_ + "/" + format(file_time_) + ".block?type="+type_
                    + "&vvid="+ vvid_ + "&platform=" + platform_;
                }

                return ec;
            }

            void on_seg_beg(
                size_t segment)
            {
                live_demuxer_->seg_beg(segment);
            }

            void on_seg_close(
                size_t segment)
            {
                live_demuxer_->seg_end(segment);

                LOG_S(framework::logger::Logger::kLevelDebug, 
                    "[on_seg_close] segment: " << segment << ", file_time_: " << file_time_);
            }

            void on_error(
                boost::system::error_code & ec)
            {
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "[on_error] ec: " << ec.message());

                if (ec == source_error::no_more_segment) {
                    if (!live_port_ )
                    {
                        next_segment();
                    }
                    ec.clear();
                    add_segment(ec);
                    clear_readed_segment(ec);
                } else if (ec == http_error::not_found) {
                    pause(5 * 1000);
                    ec.clear();
                    //ec = boost::asio::error::would_block;
                }
            }

        public:
            void set_id(std::string const & name)
            {
                std::string::size_type slash = name.find('|');
                if (slash == std::string::npos) 
                {
                    return;
                }

                key_ = name.substr(0, slash);
                std::string url = name.substr(slash + 1);

                //解URL
                //std::string   temp_host = "http://host/";
                //url = temp_host + url;
                url = framework::string::Url::decode(url);
                framework::string::Url request_url(url);
                id_ = request_url.path().substr(1);

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

                platform_ = request_url.param("platform");
                if(platform_.empty())
                {
                    platform_ = dns_live2_platform;
                }

                type_ = request_url.param("type");
                if(type_.empty())
                {
                    type_ = dns_live2_type;
                }
                vvid_ = request_url.param("vvid");
                if(vvid_.empty())
                {
                    size_t vvid = rand();
                    vvid_ = format(vvid);
                }
            }
            
            framework::string::Url get_play_url()
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_live2_play_server.host());
                url.svc(dns_live2_play_server.svc());
                url.path("/boxplay.api");
                url.param("auth","55b7c50dc1adfc3bcabe2d9b2015e35c");
                url.param("id",id_);
 
                url.param("type", type_);
                url.param("vvid", vvid_);                                                                                      
                url.param("platform", platform_);

                return url;
            }

            void set_play_info(
                Live2PlayInfo const & play_info)
            {
                play_info_ = play_info;

                local_time_ = Time::now();

                server_time_ = play_info_.dt.st_t;
                //rate_ = play_info_.channel.stream.video.bitrate;
                interval_ = play_info_.channel.stream.interval;
                if(bwtype_ == -1)
                    bwtype_ =play_info_.dt.bwt;
                stream_id_ = play_info_.channel.stream.video.rid;
                rid_.push_back(stream_id_);
                rate_.push_back(play_info_.channel.stream.video.bitrate);
                file_time_ = server_time_ - play_info_.channel.stream.delay;
                file_time_ = file_time_ / interval_ * interval_;

            }

            void set_file_time(boost::uint32_t iTime,bool bSign)  //sign ture +++  false ---
            {
                file_time_ = server_time_ - play_info_.channel.stream.delay;

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

            void next_segment()
            {
                // 当前分段已经下载的时间
                file_time_ += interval_;
                // 当前已经播放时长
                /*boost::int64_t total_seconds = (Time::now() - local_time_).total_seconds();
                if (total_seconds + server_time_ > file_time_ + play_info_.delay_play_time * 2) {
                    // 跳段时，保证比正常播放时间延迟 delay_play_time
                    while (total_seconds + server_time_ > file_time_ + play_info_.delay_play_time) {
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

            framework::network::NetName const & get_http_proxy() const
            {
                return proxy_addr_;
            }

            void set_live_demuxer(
                Live3Demuxer * live_demuxer)
            {
                live_demuxer_ = live_demuxer;
            }

            size_t segment() const
            {
                return write_segment();
            }

            std::string const & get_id() const
            {
                return id_;
            }

            std::string const & get_uuid() const
            {
                return channel_;
            }

            boost::uint16_t interval() const
            {
                return interval_;
            }

        private:
            boost::uint16_t live_port_;
            framework::network::NetName proxy_addr_;

        private:
            std::string key_;
            std::string id_;
            std::string name_;
            std::string channel_;
            std::string stream_id_;

            std::string platform_;
            std::string type_;
            std::string vvid_;

            Live2PlayInfo play_info_;
            Time local_time_;
            time_t server_time_;
            time_t file_time_;
            boost::uint32_t seek_time_;
            boost::int32_t bwtype_;

            int index_;
            std::vector<std::string> rid_;
            std::vector<boost::uint32_t> rate_;

            boost::uint16_t interval_;

            Live3Demuxer * live_demuxer_;

            boost::uint32_t seq_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

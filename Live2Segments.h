// Live2Segments.h

#ifndef _PPBOX_DEMUX_LIVE2_SEGMENTS_H_
#define _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

#include "ppbox/demux/source/HttpBufferList.h"
#include "ppbox/demux/Live2Demuxer.h"
#include "ppbox/demux/Serialize.h"

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
        static const NetName dns_live2_jump_server("(tcp)(v4)dt.api.pplive.com:80");
#else
        static const NetName dns_live2_jump_server("(tcp)(v4)jump.150hi.com:80");
#endif

        struct Live2JumpInfo
        {
            std::vector<std::string> server_hosts;
            util::serialization::UtcTime server_time;
            boost::uint16_t delay_play_time;
            std::string channelGUID;
            std::string server_limit;

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(server_hosts)
                    & SERIALIZATION_NVP(server_time)
                    & SERIALIZATION_NVP(delay_play_time)
                    & SERIALIZATION_NVP(server_limit)
                    & SERIALIZATION_NVP(channelGUID);
            }
        };

        static std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        class Live2Segments
            : public HttpBufferList<Live2Segments>
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live2Segments", 0);

        public:
            Live2Segments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : HttpBufferList<Live2Segments>(io_svc, buffer_size, prepare_size)
                , live_port_(live_port)
                , server_time_(0)
                , file_time_(0)
                , index_(0)
                , interval_(10)
                , live_demuxer_(NULL)
            {
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
                        addr.host(host_);
                        addr.port(framework::string::parse<boost::uint16_t>(svc_));
                    } else {
                        addr.host("127.0.0.1");
                        addr.port(live_port_);
                    }
                    head.host.reset(addr_host(addr));
                    head.connection = util::protocol::http_filed::Connection::keep_alive;
                }

                //TODO:
                //head.path = "/" + pptv::base64_encode(url_, key_);
                head.path = "/live2/" + stream_id_ + "/" + format(file_time_) + ".block";

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

                if (write_at_end()) {
                    LOG_S(framework::logger::Logger::kLevelDebug, "[on_seg_close] write_at_end");

                    update();
                }

                LOG_S(framework::logger::Logger::kLevelDebug, 
                    "[on_seg_close] segment: " << segment << ", file_time_: " << file_time_);
            }

            bool recoverable(
                boost::system::error_code const & ec)
            {
                if (ec == http_error::not_found) {
                    pause(5 * 1000);
                    return true;
                }

                return util::protocol::HttpClient::recoverable(ec);
            }

        public:
            void set_url_key(
                std::string const & key, 
                std::string const & url)
            {
                key_ = key;
                url_ = pptv::base64_decode(url, key);
                if (!url_.empty()) {
                    map_find(url_, "name", name_, "&");
                    map_find(url_, "channel", channel_, "&");
                }

                name_ = "CCTV";
                channel_ = "CCTV";

                rid_.push_back("092ada2421d58467abc6b18b741fa88e");
                rid_.push_back("192ada2421d58467abc6b18b741fa88e");
                rid_.push_back("292ada2421d58467abc6b18b741fa88e");
                rate_.push_back(300 * 1024);
                rate_.push_back(400 * 1024);
                rate_.push_back(450 * 1024);

                stream_id_ = rid_[0];
            }

            framework::string::Url get_jump_url()
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_live2_jump_server.host());
                url.svc(dns_live2_jump_server.svc());
                //TODO:
                //url.path("/live2/" + stream_id_);
                url.path("/%cc%a9%cc%b9%c4%e1%bf%cb%ba%c5(%c0%b6%b9%e2).mp4dt");

                return url;
            }

            void set_jump_info(
                Live2JumpInfo const & jump_info)
            {
                jump_info_ = jump_info;

                StringToken st(jump_info.server_hosts[0], ":", false);
                std::string h;
                error_code ec;
                if (!st.next_token(h, ec)) {
                    if (!h.empty()) {
                        host_ = h;
                    }
                    if (!st.remain().empty()) {
                        svc_ = st.remain();
                    }
                }

                local_time_ = Time::now();

                server_time_ = jump_info_.server_time.to_time_t();
                file_time_ = server_time_ - jump_info_.delay_play_time;

                //TODO:需要设计上配合
                file_time_ = file_time_ / interval_ * interval_;
            }

            void update()
            {
                int n = 0;
                if (file_time_ > server_time_
                    && (Time::now() - local_time_).total_seconds() > (file_time_ - server_time_)) {
                        n = ((Time::now() - local_time_).total_seconds() - (file_time_ - server_time_)) / interval_ * interval_;
                }
                if (n == 0)
                    n = 1;

                file_time_ += (n * interval_);

                boost::system::error_code ec;
                clear_readed_segment(ec);
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

            size_t segment() const
            {
                return write_segment();
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

        private:
            boost::uint16_t live_port_;
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

            std::string host_;
            std::string svc_;
            boost::uint16_t interval_;

        private:
            Live2Demuxer * live_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE2_SEGMENTS_H_

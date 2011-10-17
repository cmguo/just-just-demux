// VodSegments.h

#ifndef _PPBOX_DEMUX_VOD_SEGMENTS_H_
#define _PPBOX_DEMUX_VOD_SEGMENTS_H_

#include "ppbox/demux/source/HttpBufferList.h"
#include "ppbox/demux/VodDemuxer.h"
#include "ppbox/demux/VodInfo.h"

#include <util/protocol/pptv/TimeKey.h>
#include <util/serialization/NVPair.h>
using namespace util::protocol;

#include <framework/timer/ClockTime.h>
using namespace framework::string;
using namespace framework::network;
using namespace framework::system::logic_error;
using namespace framework::timer;

using namespace boost::system;

#ifndef PPBOX_DNS_VOD_JUMP
#define PPBOX_DNS_VOD_JUMP "(tcp)(v4)jump.150hi.com:80"
#endif

#ifndef PPBOX_DNS_VOD_DRAG
#define PPBOX_DNS_VOD_DRAG "(tcp)(v4)drag.150hi.com:80"
#endif

namespace ppbox
{
    namespace demux
    {

//#ifdef API_PPLIVE
//        static const NetName dns_vod_jump_server("(tcp)(v4)dt.api.pplive.com:80");
//       static const NetName dns_vod_drag_server("(tcp)(v4)drag.api.pplive.com:80");
//#else
        static const NetName dns_vod_jump_server(PPBOX_DNS_VOD_JUMP);
        static const NetName dns_vod_drag_server(PPBOX_DNS_VOD_DRAG);
//#endif

        static inline std::string addr_host(
            NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        class VodSegments
            : public HttpBufferList<VodSegments>
        {
        public:
            VodSegments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t vod_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : HttpBufferList<VodSegments>(io_svc, buffer_size, prepare_size)
                , vod_port_(vod_port)
                , first_seg_(true)
                , use_backup_drag_(false)
                , bwtype_(0)
                , url_("http://localhost/")
                , max_dl_speed_(boost::uint32_t(-1))
            {
            }

        public:
            error_code get_request(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                framework::network::NetName & addr, 
                util::protocol::HttpRequest & request, 
                error_code & ec)
            {
                vod_demuxer_->seg_beg(segment);

                util::protocol::HttpRequestHead & head = request.head();
                ec = error_code();
                if (segment < segments_.size()) {
                    if (vod_port_ 
                        && get_segment_num_try(segment, ec) > 3) {
                            first_seg_ = true;
                            vod_port_ = 0;
                    }
                    if (first_seg_) {
                        first_seg_ = false;
                        url_.host(server_host_.host());
                        url_.svc(server_host_.svc());
                        if (vod_port_) {
                            set_time_out(0, ec);
                            addr.host("127.0.0.1");
                            addr.port(vod_port_);
                            head.host.reset(addr_host(addr));
                        } else {
                            set_time_out(5 * 1000, ec);
                            if (proxy_addr_.host().empty()) {
                                addr = server_host_;
                            } else {
                                addr = proxy_addr_;
                            }
                            head.host.reset(addr_host(server_host_));
                            if (head.pragma.empty())
                                head.pragma.push_back("Client=PPLiveVA/1,5,2,1");
                        }
                    }
                    url_.path("/" + format(segment) + "/" + name_);
                    url_.param("key", get_key());
                    if (vod_port_) {
                        Url url("http://localhost/ppvaplaybyopen?");
                        url.param("url", url_.to_string());
                        url.param("rid", segments_[segment].va_rid);
                        url.param("blocksize", format(segments_[segment].block_size));
                        url.param("filelength", format(segments_[segment].file_length));
                        url.param("headlength", format(segments_[segment].head_length));
                        url.param("autoclose", "false");
                        boost::uint32_t buf_time = vod_demuxer_->stat().get_buf_time();
                        url.param("drag", buf_time < 15000 ? "1" : "0");
                        url.param("headonly", end <= segments_[segment].head_length ? "1" : "0");
                        url.param("BWType", format(bwtype_));

                        url.param("blocknum", format(segments_[segment].block_num));

                        url.encode();
                        head.path = url.path_all();
                    } else {
                        head.path = url_.path_all();
                    }

                    LOG_S(framework::logger::Logger::kLevelDebug, "Segment url: " << url_.to_string());
                } else {
                    ec = item_not_exist;
                }
                return ec;
            }

            void on_seg_close(
                size_t segment)
            {
                vod_demuxer_->seg_end(segment);
            }

        public:
            void set_name(
                std::string const & name)
            {
                name_ = name;
            }

            framework::string::Url get_jump_url()
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_vod_jump_server.host());
                url.svc(dns_vod_jump_server.svc());
                url.path("/" + name_ + "dt");

                return url;
            }

            framework::string::Url get_drag_url() const
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_vod_drag_server.host());
                url.svc(dns_vod_drag_server.svc());
                url.path("/" + name_ + "0drag");
                url.param("type", "ppbox");
                return url;
            }

            void set_server_host_time(
                NetName const & server_host, 
                time_t time,
                int bwtype)
            {
                server_host_ = server_host;
                server_time_ = time;
                local_time_ = Time::now();
                bwtype_ = bwtype;
            }

            NetName const & get_server_host() const
            {
                return server_host_;
            }

            void set_segments(
                std::vector<VodSegmentNew> const & segments)
            {
                segments_ = segments;
            }

            void set_http_proxy(
                NetName const & addr)
            {
                proxy_addr_ = addr;
            }

            void set_max_dl_speed(
                boost::uint32_t speed)
            {
                max_dl_speed_ = speed;
            }

            size_t segment() const
            {
                return write_segment();
            }

            void set_vod_demuxer(
                VodDemuxer * vod_demuxer)
            {
                vod_demuxer_ = vod_demuxer;
            }

        private:
            std::string get_key() const
            {
                return pptv::gen_key_from_time(server_time_ + (Time::now() - local_time_).total_seconds());
            }

        private:
            boost::uint16_t vod_port_;
            std::string name_;
            NetName server_host_;
            NetName proxy_addr_;
            bool first_seg_;
            bool use_backup_drag_;
            int bwtype_;
            Time local_time_;
            time_t server_time_;
            Url url_;
            std::vector<VodSegmentNew> segments_;
            boost::uint32_t max_dl_speed_;

        private:
            VodDemuxer * vod_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SEGMENTS_H_

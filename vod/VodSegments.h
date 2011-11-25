// VodSegments.h

#ifndef _PPBOX_DEMUX_VOD_SEGMENTS_H_
#define _PPBOX_DEMUX_VOD_SEGMENTS_H_

#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/vod/VodInfo.h"

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
            : public HttpSource
        {
        public:
            VodSegments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t vod_port)
                : HttpSource(io_svc)
                , vod_port_(vod_port)
                , first_seg_(true)
                , bwtype_(0)
                , url_("http://localhost/")
                , max_dl_speed_(boost::uint32_t(-1))
                , vod_demuxer_(NULL)
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
                util::protocol::HttpRequestHead & head = request.head();
                ec = error_code();
                if (segment < segments_.size()) {
                    if (vod_port_ 
                        && HttpSource::buffer_->num_try() > 3) {
                            HttpSource::buffer_->set_total_req(1);
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
                        boost::uint32_t buf_time = vod_demuxer_->buf_time();
                        url.param("drag", buf_time < 15000 ? "1" : "0");
                        url.param("headonly", end <= segments_[segment].head_length ? "1" : "0");
                        url.param("BWType", format(bwtype_));
                        //url.param("BWType", "0");
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

            void on_seg_beg(
                size_t segment)
            {
                vod_demuxer_->seg_beg(segment);
            }

            void on_seg_end(
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

            void add_segment(
                VodSegmentNew & segment)
            {
                segments_.push_back(segment);
                boost::system::error_code ec;
                HttpSource::buffer_->add_request(ec);
            }

            std::vector<VodSegmentNew> const & segments_info() const
            {
                return segments_;
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

            SegmentPosition const & segment() const
            {
                return HttpSource::buffer_->write_segment();
            }

            void set_vod_demuxer(
                VodDemuxer * vod_demuxer)
            {
                vod_demuxer_ = vod_demuxer;
            }

            void on_error(
                boost::system::error_code & ec)
            {
                if (ec == util::protocol::http_error::keepalive_error) {
                    ec.clear();
                    set_http_connection(util::protocol::http_field::Connection::close);
                    HttpSource::buffer_->set_total_req(1);
                } else if (ec == boost::asio::error::connection_refused) {
                    ec.clear();
                    HttpSource::buffer_->increase_req();
                }
            }

        private:
            size_t segment_count() const
            {
                return segments_.size();
            }

            boost::uint64_t segment_size(
                size_t segment)
            {
                return segments_[segment].file_length;
            }

            boost::uint64_t segment_time(
                size_t segment)
            {
                return segments_[segment].duration;
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
            int bwtype_;
            Time local_time_;
            time_t server_time_;
            Url url_;
            std::vector<VodSegmentNew> segments_;
            boost::uint32_t max_dl_speed_;

            VodDemuxer * vod_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SEGMENTS_H_
// LiveSegments.h

#ifndef _PPBOX_DEMUX_LIVE_SEGMENTS_H_
#define _PPBOX_DEMUX_LIVE_SEGMENTS_H_

#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/live/LiveDemuxer.h"

#include <util/protocol/pptv/Base64.h>
#include <util/serialization/NVPair.h>
#include <util/serialization/stl/vector.h>
using namespace util::protocol;

#include <framework/string/Slice.h>
#include <framework/string/Url.h>
using namespace framework::string;
using namespace framework::network;
using namespace framework::system::logic_error;

using namespace boost::system;

#ifndef PPBOX_DNS_LIVE_JUMP
#define PPBOX_DNS_LIVE_JUMP "(tcp)(v4)livejump.150hi.com:80"
#endif

namespace ppbox
{
    namespace demux
    {

//#ifdef API_PPLIVE
//        static const NetName dns_live_jump_server("(tcp)(v4)j.api.pplive.com:80");
//#else
        static const NetName dns_live_jump_server(PPBOX_DNS_LIVE_JUMP);
//#endif

        struct LiveJumpInfo
        {
            std::vector<std::string> server_hosts;
            std::string proto_type;
            size_t buffer_size;

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(server_hosts)
                    & SERIALIZATION_NVP(proto_type)
                    & SERIALIZATION_NVP(buffer_size);
            }
        };

        static std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        class LiveSegments
            : public HttpSource
        {
        public:
            LiveSegments(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port)
                : HttpSource(io_svc)
                , live_port_(live_port)
                , num_del_(0)
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
                util::protocol::HttpRequestHead & head = request.head();
                ec = error_code();

                buffer()->set_max_try(1);
                set_time_out(0, ec);
                if (!proxy_addr_.host().empty()) {
                    addr = proxy_addr_;
                } else {
                    addr.host("127.0.0.1");
                    addr.port(live_port_);
                }
                head.host.reset(addr_host(addr));
                head.path = "/" + pptv::base64_encode(url_, key_);

                return ec;
            }

            virtual DemuxerType::Enum demuxer_type() const
            {
                return DemuxerType::asf;
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
            }

            virtual void on_error(
                boost::system::error_code & ec)
            {
                if (ec == boost::asio::error::eof) {
                    ec.clear();
                } else if (ec == boost::asio::error::connection_refused) {
                    ec.clear();
                    buffer()->increase_req();
                }
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
            }

            framework::string::Url get_jump_url()
            {
                framework::string::Url url("http://localhost/");
                url.host(dns_live_jump_server.host());
                url.svc(dns_live_jump_server.svc());
                url.path("/live1/" + Url::encode(channel_));

                return url;
            }

            void set_jump_info(
                LiveJumpInfo const & jump_info)
            {
                jump_info_ = jump_info;
                for (size_t i = 0; i < jump_info_.server_hosts.size(); ++i) {
                    url_ += "&s=" + jump_info_.server_hosts[i];
                }
                url_ += "&st=" + jump_info_.proto_type;
                url_ += "&sl=" + format(jump_info_.buffer_size);
            }

            void set_http_proxy(
                framework::network::NetName const & addr)
            {
                proxy_addr_ = addr;
            }

            void set_live_demuxer(
                LiveDemuxer * live_demuxer)
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

            DemuxerType::Enum demuxer_type()
            {
                return DemuxerType::asf;
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
                SegmentPosition & segment)
            {
                if (!segment.source) {
                    segment.source = this;
                    segment.size_beg = segment.size_beg;
                    segment.size_end = boost::uint64_t(-1);
                    segment.time_beg = segment.time_beg;
                    segment.time_end = boost::uint64_t(-1);
                } else {
                    if (segment.segment - num_del_ >= segments_.size()) {
                        segments_.push_back(segment.size_end - segment.size_beg);
                    }
                    ++segment.segment;
                    segment.size_beg = segment.size_end;
                    segment.size_end = 
                        segment_size(segment.segment) == boost::uint64_t(-1) ? 
                        boost::uint64_t(-1) : segment_size(segment.segment) + segment.size_beg;
                    if (segment.size_end == (boost::uint64_t)-1) {
                        segment.total_state = SegmentPosition::not_init;
                    } else {
                        segment.total_state = SegmentPosition::is_valid;
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
            std::deque<boost::uint64_t> segments_;
            framework::network::NetName proxy_addr_;

            std::string key_;
            std::string url_;
            std::string name_;
            std::string channel_;
            LiveJumpInfo jump_info_;

            LiveDemuxer * live_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE_SEGMENTS_H_

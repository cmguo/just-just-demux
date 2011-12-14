// HttpOneSegment.h

#include "ppbox/demux/source/HttpSource.h"

#include <framework/string/Url.h>

namespace ppbox
{
    namespace demux
    {

        class HttpOneSegment
            : public HttpSource
        {
        public:
            HttpOneSegment(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type, 
                boost::uint64_t size, 
                boost::uint64_t time)
                : HttpSource(io_svc)
                , demuxer_type_(demuxer_type)
                , size_(size)
                , time_(time)
            {
            }

        public:
            boost::system::error_code get_request(
                size_t segment, 
                boost::uint64_t & beg, 
                boost::uint64_t & end, 
                framework::network::NetName & addr, 
                util::protocol::HttpRequest & request, 
                boost::system::error_code & ec)
            {
                ec = boost::system::error_code();

                addr = url_.host_svc();
                request.head().path = url_.path_all();
                request.head().host = url_.host_svc();
                return ec;
            }

            void set_url(
                std::string const & url)
            {
                url_.from_string(url);
            }

            void set_name(
                std::string const & name)
            {
                url_.from_string(name);
            }

        public:
            DemuxerType::Enum demuxer_type() const
            {
                return demuxer_type_;
            }

            size_t segment_count() const
            {
                return 1;
            }

            boost::uint64_t segment_size(
                size_t segment)
            {
                return size_;
            }

            boost::uint64_t segment_time(
                size_t segment)
            {
                return time_;
            }

        private:
            DemuxerType::Enum demuxer_type_;
            boost::uint64_t size_;
            boost::uint64_t time_;
            framework::string::Url url_;
        };

    } // namespace demux
} // namespace ppbox

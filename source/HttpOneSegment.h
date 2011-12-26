// HttpOneSegment.h

#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/source/OneSegment.h"

#include <framework/string/Url.h>

namespace ppbox
{
    namespace demux
    {

        class HttpOneSegment
            : public OneSegmentT<HttpSource>
        {
        public:
            HttpOneSegment(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type)
                : OneSegmentT<HttpSource>(io_svc, demuxer_type)
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

            virtual void set_name(
                std::string const & name)
            {
                url_.from_string(name);
            }

        private:
            framework::string::Url url_;
        };

    } // namespace demux
} // namespace ppbox

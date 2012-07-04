#include "ppbox/demux/Common.h"
#include "ppbox/demux/source/HttpSource.h"

#include <util/protocol/http/HttpError.h>

#include <framework/string/Url.h>

#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;

namespace ppbox
{
    namespace demux
    {
        FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("HttpSource", 0);

        HttpSource::HttpSource(
            boost::asio::io_service & io_svc)
            : Source(io_svc)
            , http_(io_svc)
        {
            addr_.svc("80");
            util::protocol::HttpRequestHead & head = request_.head();
            head["Accept"] = "{*.*}";
#ifdef MULTI_SEQ
            head.connection = util::protocol::http_field::Connection::keep_alive;
#endif
        }

        boost::system::error_code HttpSource::segment_open(
            size_t segment,
            std::string const & url,
            boost::uint64_t beg, 
            boost::uint64_t end, 
            boost::system::error_code & ec)
        {
            flag_ = true;

            framework::string::Url myUrl(url);
            addr_ = myUrl.host_svc();
            util::protocol::HttpRequestHead & head = request_.head();
            head.path = myUrl.path_all();
            if (beg != 0 || end != (boost::uint64_t)-1) {
                head.range.reset(util::protocol::http_field::Range((boost::int64_t)beg, (boost::int64_t)end));
            } else {
                head.range.reset();
            }
            std::ostringstream oss;
            head.get_content(oss);
            LOG_STR(framework::logger::Logger::kLevelDebug2, oss.str().c_str());
            http_.bind_host(addr_, ec);
            http_.open(request_, ec);
            http_.request().head().get_content(std::cout);

            return ec;
        }

        void HttpSource::segment_async_open(
            size_t segment,
            std::string const & url,
            boost::uint64_t beg, 
            boost::uint64_t end, 
            response_type const & resp)
        {
            flag_ = true;
            framework::string::Url myUrl(url);
            addr_ = myUrl.host_svc();
            boost::system::error_code ec;
            util::protocol::HttpRequestHead & head = request_.head();
            head.path = myUrl.path_all();
            if (beg != 0 || end != (boost::uint64_t)-1) {
                head.range.reset(util::protocol::http_field::Range((boost::int64_t)beg, (boost::int64_t)end));
            } else {
                head.range.reset();
            }
            std::ostringstream oss;
            head.get_content(oss);
            LOG_STR(framework::logger::Logger::kLevelDebug2, oss.str().c_str());
            http_.bind_host(addr_, ec);
            http_.async_open(request_, resp);
        }

        bool HttpSource::segment_is_open(
            boost::system::error_code & ec)
        {
            bool result = http_.is_open(ec);
            if(flag_ && result){
                util::protocol::HttpResponseHead head = http_.response().head();
                std::ostringstream oss;
                head.get_content(oss);
                LOG_STR(framework::logger::Logger::kLevelDebug2, oss.str().c_str());
                flag_ = false;
            }
            return result;
        }

        boost::system::error_code HttpSource::segment_cancel(
            size_t segment, 
            boost::system::error_code & ec)
        {
            return http_.cancel_forever(ec);
        }

        boost::system::error_code HttpSource::segment_close(
            size_t segment, 
            boost::system::error_code & ec)
        {
            on_seg_end(segment);
            return http_.close(ec);
        }

        std::size_t HttpSource::segment_read(
            const write_buffer_t & buffers,
            boost::system::error_code & ec)
        {
            std::size_t n = 0;
            if (http_.is_open(ec)) {
                n = boost::asio::read(http_, buffers, boost::asio::transfer_all(), ec);
            }

            return n;
        }

        void HttpSource::segment_async_read(
            const write_buffer_t & buffers,
            read_handle_type handler)
        {
            boost::asio::async_read(http_, buffers, boost::asio::transfer_all(), handler);
        }

        boost::uint64_t HttpSource::total(
            boost::system::error_code & ec)
        {
            boost::uint64_t n = 0;
            if (http_.is_open(ec)) {
                if (http_.response_head().content_length.is_initialized()) {
                    n = http_.response_head().content_length.get();
                } else if (http_.response_head().content_range.is_initialized()) {
                    n = http_.response_head().content_range.get().total();
                } else {
                    ec = framework::system::logic_error::no_data;
                }
            }

            return n;
        }

        boost::system::error_code HttpSource::set_non_block(
            bool non_block, 
            boost::system::error_code & ec)
        {
            return http_.set_non_block(non_block, ec);
        }

        boost::system::error_code HttpSource::set_time_out(
            boost::uint32_t time_out, 
            boost::system::error_code & ec)
        {
            //buffer()->set_time_out(time_out);
            return http_.set_time_out(time_out, ec);
        }

        void HttpSource::set_http_connection(
            util::protocol::http_field::Connection::TypeEnum connection)
        {
            request_.head().connection = connection;
        }

        bool HttpSource::continuable(
            boost::system::error_code const & ec)
        {
            return ec == boost::asio::error::would_block;
        }

        bool HttpSource::recoverable(
            boost::system::error_code const & ec)
        {
            return util::protocol::HttpClient::recoverable(ec);
        }

        void HttpSource::on_seg_beg(
            size_t segment)
        {
            return;
        }

        void HttpSource::on_seg_end(
            size_t segment)
        {
            return;
        }

        util::protocol::HttpClient::Statistics const & http_stat();

        util::protocol::HttpClient::Statistics const & HttpSource::http_stat()
        {
            return http_.stat();
        }
    } // namespace demux
} // namespace ppbox

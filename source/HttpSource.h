// HttpBufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

#include "ppbox/demux/base/BufferList.h"
#include "ppbox/demux/base/SourceBase.h"

#include <util/protocol/http/HttpClient.h>
#include <util/protocol/http/HttpError.h>

#include <framework/string/Url.h>

#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        class HttpSource
            : public SourceBase
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("HttpSegments", 0);

        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            HttpSource(
                boost::asio::io_service & io_svc)
                : SourceBase(io_svc)
                , http_(io_svc)
            {
                addr_.svc("80");
                util::protocol::HttpRequestHead & head = request_.head();
                head["Accept"] = "{*.*}";
#ifdef MULTI_SEQ
                head.connection = util::protocol::http_field::Connection::keep_alive;
#endif
            }

            boost::system::error_code segment_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                flag_ = true;
                if (!get_request(segment, beg, end, addr_, request_, ec)) {
                    util::protocol::HttpRequestHead & head = request_.head();
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
                    //LOG_STR(framework::logger::logger::kLevelDebug1, http_.response().head().heAD)
                }

                return ec;
            }

            void segment_async_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp)
            {
                flag_ = true;
                boost::system::error_code ec;
                if (!get_request(segment, beg, end, addr_, request_, ec)) {
                    util::protocol::HttpRequestHead & head = request_.head();
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
                } else {
                    resp(ec);
                }
            }

            bool segment_is_open(
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

            boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec)
            {
                return http_.cancel_forever(ec);
            }

            boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec)
            {
                on_seg_end(segment);
                return http_.close(ec);
            }

            std::size_t segment_read(
                const write_buffer_t & buffers,
                boost::system::error_code & ec)
            {
                std::size_t n = 0;
                if (http_.is_open(ec)) {
                    n = boost::asio::read(http_, buffers, boost::asio::transfer_all(), ec);
                }

                return n;
            }

            void segment_async_read(
                const write_buffer_t & buffers,
                read_handle_type handler)
            {
                boost::asio::async_read(http_, buffers, boost::asio::transfer_all(), handler);
            }

            boost::uint64_t total(
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

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)
            {
                return http_.set_non_block(non_block, ec);
            }

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)
            {
                //buffer()->set_time_out(time_out);
                return http_.set_time_out(time_out, ec);
            }

            void set_http_connection(
                util::protocol::http_field::Connection::TypeEnum connection)
            {
                request_.head().connection = connection;
            }

            bool continuable(
                boost::system::error_code const & ec)
            {
                return ec == boost::asio::error::would_block;
            }

            bool recoverable(
                boost::system::error_code const & ec)
            {
                return util::protocol::HttpClient::recoverable(ec);
            }

            util::protocol::HttpClient::Statistics const & http_stat()
            {
                return http_.stat();
            }

        public:
            virtual boost::system::error_code get_request(
                size_t segment, 
                boost::uint64_t & beg, 
                boost::uint64_t & end, 
                framework::network::NetName & addr, 
                util::protocol::HttpRequest & request, 
                boost::system::error_code & ec) = 0;

        private:
            bool flag_;
            framework::network::NetName addr_;
            util::protocol::HttpRequest request_;
            util::protocol::HttpClient http_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

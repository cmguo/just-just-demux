// HttpBufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

#include "ppbox/demux/source/BufferList.h"

#include <util/protocol/http/HttpClient.h>
#include <util/protocol/http/HttpError.h>

#include <framework/string/Url.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        template <
            typename HttpSegments
        >
        class HttpBufferList
            : public BufferList<HttpSegments>
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_USE_BASE(BufferList<HttpSegments>);

        private:
            typedef ppbox::demux::BufferList<HttpSegments> BufferList;

        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            HttpBufferList(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : BufferList(buffer_size, prepare_size)
                , http_(io_svc)
            {
                addr_.svc("80");
                util::protocol::HttpRequestHead & head = request_.head();
                head["Accept"] = "{*.*}";
            }

            //friend class ppbox::demux::BufferList<HttpBufferList<HttpSegments> >;

            //
            boost::system::error_code open_segment(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                if (!segments().get_request(segment, beg, end, addr_, request_, ec)) {
                    util::protocol::HttpRequestHead & head = request_.head();
                    if (beg != 0 || end != (boost::uint64_t)-1) {
                        head.range.reset(util::protocol::http_filed::Range((boost::int64_t)beg, (boost::int64_t)end));
                    } else {
                        head.range.reset();
                    }
                    std::ostringstream oss;
                    head.get_content(oss);
                    LOG_STR(framework::logger::Logger::kLevelDebug1, oss.str().c_str());
                    http_.reopen(addr_, request_, ec);
                }

                return ec;
            }

            void async_open_segment(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp)
            {
                boost::system::error_code ec;
                if (!segments().get_request(segment, beg, end, addr_, request_, ec)) {
                    util::protocol::HttpRequestHead & head = request_.head();
                    if (beg != 0 || end != (boost::uint64_t)-1) {
                        head.range.reset(util::protocol::http_filed::Range((boost::int64_t)beg, (boost::int64_t)end));
                    } else {
                        head.range.reset();
                    }
                    std::ostringstream oss;
                    head.get_content(oss);
                    LOG_STR(framework::logger::Logger::kLevelDebug1, oss.str().c_str());

                    http_.async_open(request_, resp);
                } else {
                    resp(ec);
                }
            }

            bool is_open(
                boost::system::error_code & ec)
            {
                bool is_success = http_.is_open(ec);

                return is_success;
            }

            boost::system::error_code cancel_segment(
                size_t segment, 
                boost::system::error_code & ec)
            {
                return http_.cancel(ec);
            }

            boost::system::error_code close_segment(
                size_t segment, 
                boost::system::error_code & ec)
            {
                segments().on_seg_close(segment);
                return http_.close(ec);
            }

            boost::system::error_code poll_read(
                boost::system::error_code & ec)
            {
                http_.poll(ec);
                return ec;
            }

            template <typename MutableBufferSequence>
            std::size_t read_some(
                const MutableBufferSequence & buffers,
                boost::system::error_code & ec)
            {
                std::size_t n = 0;
                if (http_.is_open(ec)) {
                    n = http_.read_some(buffers, ec);
                }

                return n;
            }

            template <typename MutableBufferSequence, typename ReadHandler>
            void async_read_some(
                const MutableBufferSequence & buffers,
                ReadHandler handler)
            {
                http_.async_read_some(buffers, handler);
            }

            boost::uint64_t total(
                boost::system::error_code & ec)
            {
                boost::uint64_t n = 0;
                if (http_.is_open(ec)) {
                    if (http_.get_response_head().content_length.is_initialized()) {
                        n = http_.get_response_head().content_length.get();
                    } else if (http_.get_response_head().content_range.is_initialized()) {
                        n = http_.get_response_head().content_range.get().total();
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
                BufferList::set_time_out(time_out);
                return http_.set_time_out(time_out, ec);
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

        private:
            HttpSegments & segments()
            {
                return static_cast<HttpSegments &>(*this);
            }

        private:
            framework::network::NetName addr_;
            util::protocol::HttpRequest request_;
            util::protocol::HttpClient http_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

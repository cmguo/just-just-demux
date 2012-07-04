// HttpSource.h
#ifndef _PPBOX_DEMUX_HTTPSOURCE_H_
#define _PPBOX_DEMUX_HTTPSOURCE_H_

#include "ppbox/demux/base/Source.h"

#include <util/protocol/http/HttpClient.h>

namespace ppbox
{
    namespace demux
    {

        class HttpSource
            : public Source
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            HttpSource(
                boost::asio::io_service & io_svc);

            boost::system::error_code segment_open(
                size_t segment,
                std::string const & url,
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec);

            void segment_async_open(
                size_t segment,
                std::string const & url,
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp);
           
            bool segment_is_open(
                boost::system::error_code & ec);
            
            boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec);
         
            boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec);
            
            std::size_t segment_read(
                const write_buffer_t & buffers,
                boost::system::error_code & ec);
            

            void segment_async_read(
                const write_buffer_t & buffers,
                read_handle_type handler);
           
            boost::uint64_t total(
                boost::system::error_code & ec);
            
            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec);
           
            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec);
           
            void set_http_connection(
                util::protocol::http_field::Connection::TypeEnum connection);
           
            bool continuable(
                boost::system::error_code const & ec);

            bool recoverable(
                boost::system::error_code const & ec);

            void on_seg_beg(
                size_t segment);

            void on_seg_end(
                size_t segment);
            util::protocol::HttpClient::Statistics const & http_stat();

        private:
            bool flag_;
            framework::network::NetName addr_;
            util::protocol::HttpRequest request_;
            util::protocol::HttpClient http_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

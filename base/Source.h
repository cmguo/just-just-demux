// Source.h
#ifndef _PPBOX_DEMUX_SOURCE__H_
#define _PPBOX_DEMUX_SOURCE__H_

#include <util/buffers/Buffers.h>

#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {
        class Source
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

            typedef boost::function<void(
                boost::system::error_code const &,
                size_t)
            > read_handle_type;

            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

        public:
            Source(boost::asio::io_service & io_svc)
                :ios_(io_svc)
            {}

            virtual ~Source(){}

            virtual boost::system::error_code segment_open(
                size_t segment, 
                std::string const & url,
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)= 0;

            virtual void segment_async_open(
                size_t segment,
                std::string const & url,
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp)= 0;

            virtual bool segment_is_open(
                boost::system::error_code & ec)= 0;

            virtual boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec)= 0;

            virtual boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec)= 0;

            virtual std::size_t segment_read(
                const write_buffer_t & buffers,
                boost::system::error_code & ec)= 0;

            virtual void segment_async_read(
                const write_buffer_t & buffers,
                read_handle_type handler)= 0;

            virtual boost::uint64_t total(
                boost::system::error_code & ec)= 0;

            virtual boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)= 0;

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)= 0;

            virtual bool continuable(
                boost::system::error_code const & ec)= 0;

            virtual bool recoverable(
                boost::system::error_code const & ec) = 0;

            virtual void on_seg_beg(
                size_t segment) = 0;

            virtual void on_seg_end(
                size_t segment) = 0;
        protected:
            boost::asio::io_service &ios_service() 
            {
                return ios_;
            }
        private:
            boost::asio::io_service &ios_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_HTTP_BUFFER_LIST_H_

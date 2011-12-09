// DescriptorSource.h

#ifndef _PPBOX_DEMUX_SOURCE_DESCRIPTOR_SOURCE_H_
#define _PPBOX_DEMUX_SOURCE_DESCRIPTOR_SOURCE_H_

#include "ppbox/demux/source/BufferList.h"

#include <framework/system/ErrorCode.h>

#ifndef BOOST_WINDOWS_API
#  include <boost/asio/posix/stream_descriptor.hpp>
typedef boost::asio::posix::stream_descriptor descriptor;
#else
#  include <boost/asio/windows/stream_handle.hpp>
typedef boost::asio::windows::stream_handle descriptor;
#endif

namespace ppbox
{
    namespace demux
    {

        >
        class DescriptorSource
            : public SourceBase
        {
        public:
            typedef ppbox::demux::BufferList<DescriptorSegments> BufferList;

            typedef descriptor::native_type native_descriptor;

        public:
            DescriptorSource(
                boost::asio::io_service & io_svc)
                : SourceBase(io_svc)
                , descriptor_(io_svc)
                , is_open_(false)
            {
            }

            boost::system::error_code open_segment(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                native_descriptor nd;
                if (is_open_)
                    descriptor_.close(ec);
                if (beg != 0 && end != boost::uint64_t(-1)) {
                    ec = framework::system::logic_error::unknown_error;
                    return ec;
                }
                if (!segments().get_native_descriptor(segment, nd, ec)) {
                    descriptor_.assign(nd);
                    is_open_ = descriptor_.is_open();
                    if (!is_open_) {
                        ec = framework::system::last_system_error();
                        if (!ec) {
                            ec = framework::system::logic_error::unknown_error;
                        }
                    }
                }
                return ec;
            }

            bool is_open(
                boost::system::error_code & ec)
            {
                return is_open_;
            }

            boost::system::error_code cancel_segment(
                size_t segment, 
                boost::system::error_code & ec)
            {
                descriptor_.close();
                return ec = boost::system::error_code();
            }

            boost::system::error_code close_segment(
                size_t segment, 
                boost::system::error_code & ec)
            {
                descriptor_.close();
                is_open_ = false;
                return ec = boost::system::error_code();
            }

            boost::system::error_code poll_read(
                boost::system::error_code & ec)
            {
                return ec = boost::system::error_code();
            }

            template <typename MutableBufferSequence>
            std::size_t read_some(
                const MutableBufferSequence & buffers,
                boost::system::error_code & ec)
            {
                return descriptor_.read_some(buffers, ec);
            }

            boost::uint64_t total(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::no_data;
                return 0;
            }

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)
            {
#ifndef BOOST_WINDOWS_API
                boost::asio::posix::descriptor_base::non_blocking_io cmd(non_block);
                return descriptor_.io_control(cmd, ec);
#else
                return ec = boost::system::error_code();
#endif    
            }

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)
            {
                return ec = boost::system::error_code();
            }

            bool continuable(
                boost::system::error_code const & ec)
            {
                return ec == boost::asio::error::would_block;
            }

            bool recoverable(
                boost::system::error_code const & ec)
            {
                return false;
            }

        private:
            virtual boost::system::error_code get_native_descriptor(
                size_t segment, 
                native_descriptor & file, 
                boost::system::error_code & ec) = 0
            
        private:
            descriptor descriptor_;
            bool is_open_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_DESCRIPTOR_SOURCE_H_

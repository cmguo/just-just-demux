// FileBufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_

#include "ppbox/demux/source/BufferList.h"

#include <framework/system/ErrorCode.h>

#include <boost/asio/buffer.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>

namespace ppbox
{
    namespace demux
    {

        template <
            typename FileSegments
        >
        class FileBufferList
            : public BufferList<FileSegments>
        {
        private:
            typedef ppbox::demux::BufferList<FileSegments> BufferList;

        public:
            FileBufferList(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : BufferList(buffer_size, prepare_size)
                , is_open_(false)
            {
            }

            friend class ppbox::demux::BufferList<FileSegments>;

            boost::system::error_code open_segment(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                boost::filesystem::path ph;
                if (is_open_)
                    file_.close();
                if (!segments().get_file_name(segment, ph, ec)) {
                    file_.open(ph.file_string().c_str(), std::ios::binary | std::ios::in);
                    is_open_ = file_.is_open();
                    if (!is_open_) {
                        ec = framework::system::last_system_error();
                        if (!ec) {
                            ec = framework::system::logic_error::unknown_error;
                        }
                    } else {
                        if (beg > 0)
                            file_.seekg(beg, std::ios_base::beg);
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
                file_.close();
                return ec = boost::system::error_code();
            }

            boost::system::error_code close_segment(
                size_t segment, 
                boost::system::error_code & ec)
            {
                file_.close();
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
                size_t read_t = 0,read_cnt = 0;
                typedef typename MutableBufferSequence::const_iterator iterator;
                for (iterator iter = buffers.begin(); iter != buffers.end(); ++iter) {
                    char * buf_ptr = boost::asio::buffer_cast<char *>(*iter);
                    size_t buf_size = boost::asio::buffer_size(*iter);
                    file_.read(buf_ptr,buf_size);
                    read_t = file_.gcount();
                    read_cnt += read_t;
                    if (read_t != buf_size) {
                        break;
                    }
                }
                if (read_cnt > 0) {
                    ec = boost::system::error_code();
                } else if (file_.eof()) {
                    ec = boost::asio::error::eof;
                } else {
                    ec = framework::system::last_system_error();
                    if (!ec) {
                        ec = framework::system::logic_error::no_data;
                    }
                }
                return read_cnt;
            }

            boost::uint64_t total(
                boost::system::error_code & ec)
            {
                size_t cur;
                boost::uint64_t file_length;
                cur = file_.tellg();
                file_.seekg(0, std::ios_base::end);

                file_length = file_.tellg();
                file_.seekg(cur, std::ios_base::beg);

                return file_length;
            }

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)
            {
                return ec = boost::system::error_code();
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
            FileSegments & segments()
            {
                return static_cast<FileSegments &>(*this);
            }

        private:
            std::ifstream file_;
            bool is_open_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_

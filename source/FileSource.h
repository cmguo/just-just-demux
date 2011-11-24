// FileBufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_

#include "ppbox/demux/source/BufferList.h"
#include "ppbox/demux/source/SourceBase.h"

#include <framework/system/ErrorCode.h>

#include <boost/asio/buffer.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>

namespace ppbox
{
    namespace demux
    {

        class FileSource
            : public SourceBase
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            FileSource(
                boost::asio::io_service & io_svc)
                :SourceBase(io_svc)
                , is_open_(false)
            {
            }

            boost::system::error_code segment_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                boost::filesystem::path ph;
                if (is_open_)
                    file_.close();
                if (!get_file_name(segment, ph, ec)) {
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

            void segment_async_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp)
            {
                boost::system::error_code ec;
                segment_open(segment, beg, end, ec);
                resp(ec);
            }

            bool segment_is_open(
                boost::system::error_code & ec)
            {
                return is_open_;
            }

            boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec)
            {
                file_.close();
                return ec = boost::system::error_code();
            }

            boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec)
            {
                on_seg_close(segment);
                file_.close();
                is_open_ = false;
                return ec = boost::system::error_code();
            }

            std::size_t segment_read(
                const write_buffer_t & buffers,
                boost::system::error_code & ec)
            {
                size_t read_t = 0, read_cnt = 0;
                typedef write_buffer_t::const_iterator iterator;
                for (iterator iter = buffers.begin(); iter != buffers.end(); ++iter) {
                    char * buf_ptr = boost::asio::buffer_cast<char *>(*iter);
                    size_t buf_size = boost::asio::buffer_size(*iter);
                    file_.read(buf_ptr, buf_size);
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

            void segment_async_read(
                const write_buffer_t & buffers,
                read_handle_type handler)
            {
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

        public:
            virtual boost::system::error_code get_file_name(
                size_t segment, 
                boost::filesystem::path & file, 
                boost::system::error_code & ec)
            {
                return boost::system::error_code();
            }

            virtual void set_buffer_list(
                BufferList * buffer)
            {
                buffer_ = buffer;
            }

        protected:
            BufferList * buffer_;

        private:
            std::ifstream file_;
            bool is_open_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_FILE_BUFFER_LIST_H_

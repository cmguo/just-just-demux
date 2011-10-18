// FileOneSegment.h

#include "ppbox/demux/source/FileBufferList.h"

namespace ppbox
{
    namespace demux
    {

        class FileOneSegment
            : public FileBufferList<FileOneSegment>
        {
        public:
            FileOneSegment(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : FileBufferList<FileOneSegment>(io_svc, buffer_size, prepare_size)
            {
            }

        public:
            boost::system::error_code get_file_name(
                size_t segment, 
                boost::filesystem::path & file, 
                boost::system::error_code & ec)
            {
                ec = boost::system::error_code();

                file = fpath;
                return ec;
            }

            void on_seg_beg(
                size_t segment)
            {
            }

            void on_seg_close(
                size_t segment)
            {
            }

            void set_file_name(
                boost::filesystem::path const & file)
            {
                fpath = file;
            }

            void set_name(
                std::string const & file)
            {
                fpath = file;
            }

        private:
            boost::filesystem::path fpath;
        };

    } // namespace demux
} // namespace ppbox

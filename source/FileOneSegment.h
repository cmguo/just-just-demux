// FileOneSegment.h
#ifndef _PPBOX_DEMUX_FILE_ONE_SEGMENT_H_
#define _PPBOX_DEMUX_FILE_ONE_SEGMENT_H_

#include "ppbox/demux/source/FileSource.h"

namespace ppbox
{
    namespace demux
    {

        class FileOneSegment
            : public FileSource
        {
        public:
            FileOneSegment(
                boost::asio::io_service & io_svc)
                : FileSource(io_svc)
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

        public:
            DemuxerType::Enum demuxer_type()
            {
                return demuxer_type_;
            }

        private:
            size_t segment_count() const
            {
                return 1;
            }

            boost::uint64_t segment_size(
                size_t segment)
            {
                return boost::uint64_t(-1);
            }

            boost::uint64_t segment_time(
                size_t segment)
            {
                return boost::uint64_t(-1);
            }

        private:
            boost::filesystem::path fpath;
        };

    } // namespace demux
} // namespace ppbox

#endif
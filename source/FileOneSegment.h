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
            boost::system::error_code get_segment(
                size_t index,
                Segment & segment,
                boost::system::error_code & ec)
            {
                segment = segment_;
                return ec;
            }

            Segment & operator [](
                size_t segment)
            {
                return segment_;
            }

            Segment const & operator [](
                size_t segment) const
            {
                return segment_;
            }

            size_t total_segments() const
            {
                return 1;
            }

            void set_demuxer_type(
                DemuxerType::Enum demuxer_type)
            {
                segment_.demuxer_type = demuxer_type;
            }

        private:
            boost::filesystem::path fpath;
            Segment segment_;
        };

    } // namespace demux
} // namespace ppbox

#endif

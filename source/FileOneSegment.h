// FileOneSegment.h

#ifndef _PPBOX_DEMUX_FILE_ONE_SEGMENT_H_
#define _PPBOX_DEMUX_FILE_ONE_SEGMENT_H_

#include "ppbox/demux/source/FileSource.h"
#include "ppbox/demux/source/OneSegment.h"

namespace ppbox
{
    namespace demux
    {

        class FileOneSegment
            : public OneSegmentT<FileSource>
        {
        public:
            FileOneSegment(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type)
                : OneSegmentT<FileSource>(io_svc, demuxer_type)
            {
            }

        public:
            virtual boost::system::error_code get_file_name(
                size_t segment, 
                boost::filesystem::path & file, 
                boost::system::error_code & ec)
            {
                ec = boost::system::error_code();

                file = fpath;
                return ec;
            }

            void set_file_name(
                boost::filesystem::path const & file)
            {
                fpath = file;
            }

            virtual void set_name(
                std::string const & file)
            {
                fpath = file;
            }

        private:
            boost::filesystem::path fpath;
        };

    } // namespace demux
} // namespace ppbox

#endif

// Mp4Demuxer.h

#ifndef _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_

#include "ppbox/demux/basic/BasicDemuxer.h"
#include "ppbox/demux/basic/mp4/Mp4Stream.h"

#include <ppbox/avformat/mp4/lib/Mp4File.h>
#include <ppbox/avformat/mp4/box/Mp4BoxArchive.h>

class AP4_File;

namespace ppbox
{
    namespace demux
    {

        class Mp4Demuxer
            : public BasicDemuxer
        {
        public:
#ifdef PPBOX_DEMUX_MP4_NO_TIME_ORDER
            typedef Mp4Stream::StreamOffsetList StreamList;
#else
            typedef Mp4Stream::StreamTimeList StreamList;
#endif

        public:
            Mp4Demuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~Mp4Demuxer();

        public:
            virtual boost::system::error_code open(
                boost::system::error_code & ec);

            virtual bool is_open(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

        public:
            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
            static boost::uint32_t probe(
                boost::uint8_t const * header, 
                size_t hsize);

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) const;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

        protected:
            virtual boost::uint64_t seek(
                std::vector<boost::uint64_t> & dts, 
                boost::uint64_t & delta, 
                boost::system::error_code & ec);

        private:
            bool is_open(
                boost::system::error_code & ec) const;

        private:
            ppbox::avformat::Mp4BoxIArchive archive_;

            boost::uint64_t open_step_;
            boost::uint64_t parse_offset_;
            boost::uint64_t header_offset_;

            ppbox::avformat::Mp4File file_;
            std::auto_ptr<ppbox::avformat::Mp4Box> box_;
            std::vector<Mp4Stream *> streams_;
            StreamList * stream_list_;
            //const_pointer copy_from_;
        };

        PPBOX_REGISTER_BASIC_DEMUXER("mp4", Mp4Demuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_

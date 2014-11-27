// AviDemuxer.h

#ifndef _JUST_DEMUX_BASIC_AVI_AVI_DEMUXER_H_
#define _JUST_DEMUX_BASIC_AVI_AVI_DEMUXER_H_

#include "just/demux/basic/BasicDemuxer.h"
#include "just/demux/basic/avi/AviStream.h"

#include <just/avformat/avi/lib/AviFile.h>
#include <just/avformat/avi/box/AviBoxArchive.h>

class AP4_File;

namespace just
{
    namespace demux
    {

        class AviDemuxer
            : public BasicDemuxer
        {
        public:
#ifdef JUST_DEMUX_AVI_NO_TIME_ORDER
            typedef AviStream::StreamOffsetList StreamList;
#else
            typedef AviStream::StreamTimeList StreamList;
#endif

        public:
            AviDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~AviDemuxer();

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
            just::avformat::AviBoxIArchive archive_;

            boost::uint64_t open_step_;
            boost::uint64_t parse_offset_;
            boost::uint64_t header_offset_;

            just::avformat::AviFile file_;
            std::auto_ptr<just::avformat::AviBox> box_;
            std::vector<AviStream *> streams_;
            StreamList * stream_list_;
            //const_pointer copy_from_;
        };

        JUST_REGISTER_BASIC_DEMUXER("avi", AviDemuxer);
        JUST_REGISTER_BASIC_DEMUXER("3gp", AviDemuxer);

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_AVI_AVI_DEMUXER_H_

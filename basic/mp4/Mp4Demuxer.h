// Mp4Demuxer.h

#ifndef _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_

#include "ppbox/demux/basic/BasicDemuxer.h"

#include <framework/container/OrderedUnidirList.h>

class AP4_File;

namespace ppbox
{
    namespace demux
    {

        class SampleListItem;
        struct SampleOffsetLess;
        struct SampleTimeLess;
        class Track;

        class Mp4Demuxer
            : public BasicDemuxer
        {
        public:
            typedef framework::container::OrderedUnidirList<
                SampleListItem, 
                framework::container::identity<SampleListItem>, 
                SampleOffsetLess
            > SampleOffsetList;

            typedef framework::container::OrderedUnidirList<
                SampleListItem, 
                framework::container::identity<SampleListItem>, 
                SampleTimeLess
            > SampleTimeList;

#ifdef PPBOX_DEMUX_MP4_NO_TIME_ORDER
            typedef SampleOffsetList SampleList;
#else
            typedef SampleTimeList SampleList;
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

        protected:
            virtual boost::uint32_t probe(
                boost::uint8_t const * header, 
                size_t hsize) const;

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
            boost::system::error_code parse_head(
                boost::system::error_code & ec);

            boost::system::error_code reset2(
                boost::system::error_code & ec);

        private:
            std::basic_istream<boost::uint8_t> is_;
            boost::uint64_t head_size_;
            boost::uint64_t open_step_;
            AP4_File * file_;
            std::vector<Track *> tracks_;
            boost::uint64_t bitrate_;
            SampleList * sample_list_;
            //const_pointer copy_from_;
        };

        PPBOX_REGISTER_BASIC_DEMUXER("mp4", Mp4Demuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_DEMUXER_H_

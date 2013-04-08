// MkvDemuxer.h

#ifndef _PPBOX_DEMUX_BASIC_MKV_MKV_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_MKV_MKV_DEMUXER_H_

#include "ppbox/demux/basic/BasicDemuxer.h"
#include "ppbox/demux/basic/mkv/MkvStream.h"
#include "ppbox/demux/basic/mkv/MkvParse.h"

#include <framework/system/LimitNumber.h>

namespace ppbox
{
    namespace demux
    {

        class MkvDemuxer
            : public BasicDemuxer
        {

        public:
            MkvDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~MkvDemuxer();

        public:
            virtual boost::system::error_code open(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

            virtual bool is_open(
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

            boost::system::error_code get_real_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::system::error_code get_key_sample(
                Sample & sample,
                boost::system::error_code & ec);

            boost::system::error_code get_sample_without_data(
                Sample & sample,
                boost::system::error_code & ec);

            boost::system::error_code get_end_time_sample(
                Sample & sample,
                boost::system::error_code & ec);

            bool is_video_sample(Sample & sample);

        private:
            bool find_element(
                boost::uint32_t id);

        public:
            ppbox::avformat::MkvIArchive archive_;

            size_t open_step_;
            ppbox::avformat::MkvSegmentInfo file_prop_;
            std::vector<MkvStream> streams_;
            std::vector<size_t> stream_map_; // Map index to FlvStream
            boost::uint64_t header_offset_;
            MkvParse object_parse_; 

            // for calc end time
            mutable MkvParse buffer_parse_;

            // for calc sample timestamp
            boost::uint64_t timestamp_offset_ms_;
        };

        PPBOX_REGISTER_BASIC_DEMUXER("mkv", MkvDemuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MKV_MKV_DEMUXER_H_

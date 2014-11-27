// FlvDemuxer.h

#ifndef _JUST_DEMUX_BASIC_FLV_FLV_DEMUXER_H_
#define _JUST_DEMUX_BASIC_FLV_FLV_DEMUXER_H_

#include "just/demux/basic/BasicDemuxer.h"

#include <just/avformat/flv/FlvTagType.h>
#include <just/avformat/flv/FlvMetaData.h>

#include <framework/system/LimitNumber.h>

namespace just
{
    namespace demux
    {

        class FlvStream;

        class FlvDemuxer
            : public BasicDemuxer
        {
        public:
            FlvDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~FlvDemuxer();

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

        public:
            static boost::uint32_t probe(
                boost::uint8_t const * hbytes, 
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

            boost::system::error_code get_tag(
                just::avformat::FlvTag & flv_tag,
                boost::system::error_code & ec);

        private:
            just::avformat::FlvIArchive archive_;

            just::avformat::FlvHeader flv_header_;
            just::avformat::FlvMetaData metadata_;
            std::vector<FlvStream> streams_;
            std::vector<size_t> stream_map_; // Map index to FlvStream
            just::avformat::FlvTag flv_tag_;

            size_t open_step_;
            boost::uint64_t header_offset_;
            boost::uint64_t parse_offset_;
            boost::uint64_t parse_offset2_;

            boost::uint64_t timestamp_offset_ms_;
            framework::system::LimitNumber<32> timestamp_;
        };

        JUST_REGISTER_BASIC_DEMUXER("flv", FlvDemuxer);

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_FLV_FLV_DEMUXER_H_

// FlvDemuxer.h

#ifndef _PPBOX_DEMUX_FLV_FLV_DEMUXER_H_
#define _PPBOX_DEMUX_FLV_FLV_DEMUXER_H_

#include "ppbox/demux/base/Demuxer.h"

#include "ppbox/demux/flv/FlvStream.h"

#include <framework/system/LimitNumber.h>

namespace ppbox
{
    namespace demux
    {
        class FlvDemuxer
            : public Demuxer
        {
        public:
            FlvDemuxer(
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
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::uint64_t seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

        public:
            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec);

            virtual size_t get_stream_count(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec);

        public:
            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
            virtual void set_stream(
                std::basic_streambuf<boost::uint8_t> & buf);

            virtual boost::uint64_t get_offset(
                boost::uint64_t & time, 
                boost::uint64_t & delta, 
                boost::system::error_code & ec);

        private:
            void set_time_offset(
                boost::uint64_t offset);

            boost::uint64_t get_time_offset() const;

        private:
            //boost::system::error_code parse_stream(
            //    boost::system::error_code & ec);

            void parse_metadata(
                FlvTag const & metadata_tag);

            boost::system::error_code get_tag(
                FlvTag & flv_tag,
                boost::system::error_code & ec);

        private:
            FLVArchive archive_;

            FlvHeader flv_header_;
            FlvMetadata     metadata_;
            std::vector<FlvStream> streams_;
            std::vector<size_t> stream_map_; // Map index to FlvStream
            FlvTag flv_tag_;

            boost::uint64_t open_step_;
            boost::uint64_t header_offset_;

            boost::uint64_t parse_offset_;
            boost::uint64_t timestamp_offset_ms_;
            framework::system::LimitNumber<32> timestamp_;

            boost::uint64_t current_time_;
        };

        PPBOX_REGISTER_DEMUXER("flv", FlvDemuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_DEMUXER_H_

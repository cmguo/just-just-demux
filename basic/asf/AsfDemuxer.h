// AsfDemuxer.h

#ifndef _PPBOX_DEMUX_BASIC_ASF_ASF_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_ASF_ASF_DEMUXER_H_

#include "ppbox/demux/basic/BasicDemuxer.h"
#include "ppbox/demux/basic/asf/AsfStream.h"
#include "ppbox/demux/basic/asf/AsfParse.h"

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <framework/system/LimitNumber.h>

namespace ppbox
{
    namespace demux
    {

        class AsfDemuxer
            : public ppbox::avformat::ASF_Object_Header
            , public BasicDemuxer
        {

        public:
            AsfDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~AsfDemuxer();

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
            struct ParseStatus
            {
                ParseStatus()
                    : num_packet(0)
                    , num_payload(0)
                {
                    context.packet = &packet;
                }

                ppbox::avformat::ASF_Packet packet;
                ppbox::avformat::ASF_PayloadHeader payload;
                ppbox::avformat::ASF_ParseContext context;
                boost::uint64_t offset_packet;
                boost::uint64_t offset;
                boost::uint64_t num_packet;
                boost::uint64_t num_payload;
            };

            boost::system::error_code next_packet(
                ppbox::avformat::ASFIArchive & archive, 
                ParseStatus & parse_status, 
                boost::system::error_code & ec) const;

            boost::system::error_code next_payload(
                ppbox::avformat::ASFIArchive & archive, 
                ParseStatus & parse_status, 
                boost::system::error_code & ec) const;

        public:
            ppbox::avformat::ASFIArchive archive_;

            size_t open_step_;
            ppbox::avformat::ASF_Header_Object header_;
            ppbox::avformat::ASF_File_Properties_Object_Data file_prop_;
            std::vector<AsfStream> streams_;
            std::vector<size_t> stream_map_; // Map index to AsfStream
            // std::vector<Sample> start_samples_; // send revert order
            boost::uint64_t header_offset_;

            ParseStatus object_parse_;
            std::vector<AsfParse> parses_;

            // for calc end time
            boost::uint64_t fixed_packet_length_;
            mutable ParseStatus buffer_parse_;
            framework::system::LimitNumber<32> timestamp2_;

            // for calc sample timestamp
            boost::uint64_t timestamp_offset_ms_;
        };

        PPBOX_REGISTER_BASIC_DEMUXER("asf", AsfDemuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_ASF_ASF_DEMUXER_H_

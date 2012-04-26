// AsfDemuxerBase.h

#ifndef _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/asf/AsfStream.h"

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <framework/system/LimitNumber.h>

using namespace ppbox::avformat;

namespace ppbox
{
    namespace demux
    {

        class AsfDemuxerBase
            : public ASF_Object_Header
            , public DemuxerBase
        {

        public:
            AsfDemuxerBase(
                std::basic_streambuf<boost::uint8_t> & buf)
                : DemuxerBase(buf)
                , archive_(buf)
                , open_step_(size_t(-1))
                , object_parse_(file_prop_)
                , buffer_parse_(file_prop_)
                , timestamp_offset_ms_(boost::uint32_t(-1))
            {
            }

            ~AsfDemuxerBase()
            {
            }

            boost::system::error_code open(
                boost::system::error_code & ec,
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

            boost::system::error_code close(
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            boost::uint32_t get_duration(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint64_t seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::uint64_t get_offset(
                boost::uint32_t & time, 
                boost::uint32_t & delta, 
                boost::system::error_code & ec);

            void set_stream(std::basic_streambuf<boost::uint8_t> & buf);

        private:
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
                ParseStatus(
                    ASF_File_Properties_Object_Data const & FileProperties)
                    : packet(FileProperties.MaximumDataPacketSize)
                    , num_packet(0)
                    , num_payload(0)
                {
                }

                ASF_Packet packet;
                ASF_PayloadHeader payload;
                boost::uint64_t offset_packet;
                boost::uint64_t offset;
                boost::uint32_t num_packet;
                boost::uint32_t num_payload;
                framework::system::LimitNumber<32> timestamp;
            };

            boost::system::error_code next_packet(
                ParseStatus & parse_status,  
                boost::system::error_code & ec);

            boost::system::error_code next_payload(
                ParseStatus & parse_status,  
                boost::system::error_code & ec);

        public:
            ASFIArchive archive_;

            size_t open_step_;
            ASF_Header_Object header_;
            ASF_File_Properties_Object_Data file_prop_;
            std::vector<AsfStream> streams_;
            std::vector<size_t> stream_map_; // Map index to AsfStream
            // std::vector<Sample> start_samples_; // send revert order
            ParseStatus object_parse_; 
            std::vector<ASF_PayloadHeader> object_payloads_;
            boost::uint32_t next_object_offset_; // not offset in file, just offset of this object
            bool is_discontinuity_;

            // for calc end time
            boost::uint32_t fixed_packet_length_;
            ParseStatus buffer_parse_;

            // for calc sample timestamp
            boost::uint32_t timestamp_offset_ms_;
        };

    }
}

#endif // _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_

// AsfDemuxerBase.h

#ifndef _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/asf/AsfObjectType.h"
#include "ppbox/demux/asf/AsfStream.h"

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
                , skip_type_(0)
                , fisrt_sample_dts_(boost::uint64_t(-1))
                , last_valid_sample_dts_(0)
                , last_valid_sample_itrack_(0)
                , first_next_sample_dts_(boost::uint64_t(-1))
            {
            }

            ~AsfDemuxerBase()
            {
            }

            boost::system::error_code open(
                boost::system::error_code & ec);

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
                boost::uint32_t offset_packet;
                boost::uint32_t offset;
                boost::uint32_t num_packet;
                boost::uint32_t num_payload;
            };

            boost::system::error_code next_packet(
                ParseStatus & parse_status,  
                boost::system::error_code & ec);

            boost::system::error_code next_payload(
                ParseStatus & parse_status,  
                boost::system::error_code & ec);

        public:
            ASFArchive archive_;

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

            // for calc buffer time
            boost::uint32_t fixed_packet_length_;
            ParseStatus buffer_parse_;

            // for calc sample timestamp
            boost::uint32_t skip_type_;
            boost::uint64_t fisrt_sample_dts_;
            boost::uint64_t last_valid_sample_dts_;
            boost::uint32_t last_valid_sample_itrack_;
            boost::uint64_t first_next_sample_dts_;

        };

    }
}

#endif // _PPBOX_DEMUX_ASF_ASF_DEMUXER_BASE_H_

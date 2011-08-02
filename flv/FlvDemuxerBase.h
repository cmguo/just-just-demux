// FlvDemuxerBase.h

#ifndef _PPBOX_DEMUX_FLV_FLV_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_FLV_FLV_DEMUXER_BASE_H_

#include "ppbox/demux/DemuxerBase.h"
#include "ppbox/demux/flv/FlvTagType.h"
#include "ppbox/demux/flv/FlvFormat.h"

#include <vector>

namespace ppbox
{
    namespace demux
    {
        class FlvDemuxerBase
        {
        public:
            FlvDemuxerBase(
                //std::basic_istream<boost::uint8_t, std::char_traits<boost::uint8_t> > & buf)
                std::basic_streambuf<boost::uint8_t> & buf)
                : archive_(buf)
                , open_step_((boost::uint32_t)-1)
                , header_offset_(0)
                , parse_offset_(0)
                , timestamp_offset_ms_(0)
                , reopen_(false)
            {
                streams_info_.resize(2);
            }

            ~FlvDemuxerBase()
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
                boost::uint32_t total_size, 
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint64_t seek_to(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

        private:
            boost::system::error_code parse_metadata(
                boost::uint8_t const * buffer,
                boost::uint32_t size,
                boost::system::error_code & ec);

            boost::system::error_code parse_stream(
                boost::system::error_code & ec);

            boost::system::error_code parse_amf_array(
                boost::uint8_t const *buffer,
                boost::uint32_t size,
                boost::uint32_t & length,
                boost::system::error_code & ec);

            void parse_audio_config(
                boost::uint8_t audio_config);

            boost::int32_t get_amf_string(
                boost::uint8_t const * buffer,
                boost::uint32_t size,
                std::string & value);

            boost::int32_t get_amf_bool(
                boost::uint8_t const * buffer,
                boost::uint32_t size,
                bool & value);

            boost::int32_t get_amf_number(
                boost::uint8_t const * buffer,
                boost::uint32_t size,
                double & value);

            boost::int32_t get_amf_array(
                boost::uint8_t const * buffer,
                boost::uint32_t size,
                boost::uint32_t & array_size);

            boost::int32_t get_amf_unknow(
                boost::uint8_t const * buffer,
                boost::uint32_t size);

            bool find_amf_array(
                boost::uint8_t const * p,
                boost::uint32_t size,
                boost::uint32_t & pos);

            boost::system::error_code get_tag(
                FlvTag & flv_tag,
                boost::system::error_code & ec);

        private:
            FLVArchive archive_;

            boost::uint32_t open_step_;
            FlvMetadata     metadata_;
            boost::uint32_t header_offset_;
            boost::uint32_t parse_offset_;
            boost::uint32_t timestamp_offset_ms_;
            bool reopen_;
            Sample sample_;
            std::vector<MediaInfo> streams_info_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_DEMUXER_BASE_H_

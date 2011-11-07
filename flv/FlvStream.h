// FlvStream.h

#ifndef _PPBOX_DEMUX_FLV_FLV_STREAM_H_
#define _PPBOX_DEMUX_FLV_FLV_STREAM_H_

#include "ppbox/demux/flv/FlvFormat.h"
#include "ppbox/demux/flv/FlvDataType.h"
#include "ppbox/demux/flv/FlvTagType.h"

namespace ppbox
{
    namespace demux
    {

        class FlvStream
            : public FlvTag
            , public MediaInfo
        {
        public:
            FlvStream()
                : index_to_map((boost::uint32_t)-1)
            {
            }

            FlvStream(
                FlvTag const & tag, 
                std::vector<boost::uint8_t> const & codec_data)
                : FlvTag(tag)
                , ready(false)
            {
                parse(codec_data);
            }

        private:
            void parse(
                std::vector<boost::uint8_t> const & codec_data)
            {
                if (DataSize > 0) {
                    if (TagType::FLV_TAG_TYPE_VIDEO == Type) { 
                        type = MEDIA_TYPE_VIDE;

                        switch (VideoTag.CodecID) {
                            case 0:
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = MediaInfo::video_avc_packet;
                                break;
                            default:
                                format_type = 0;
                                sub_type = VIDEO_TYPE_NONE;
                                break;
                        }
                        format_data = codec_data;
                    } else if (TagType::FLV_TAG_TYPE_AUDIO == Type) {
                        type = MEDIA_TYPE_AUDI;
                        switch (AudioTag.SoundFormat) {
                            case 0:
                                format_type = MediaInfo::audio_iso_mp4;
                                sub_type = AUDIO_TYPE_MP4A;
                                break;
                            default:
                                format_type = 0;
                                sub_type = AUDIO_TYPE_NONE;
                                break;
                        }
                        boost::uint32_t frequency[4] = {5500, 11025, 22050, 44100};
                        boost::uint32_t size[2] = {8, 16};
                        boost::uint32_t channel[2] = {1, 2};
                        audio_format.sample_rate = frequency[AudioTag.SoundRate];
                        audio_format.sample_size = size[AudioTag.SoundSize];
                        audio_format.channel_count = channel[AudioTag.SoundType];
                        format_data = codec_data;
                    }
                }
            }

        public:
            bool ready;
            boost::uint32_t index_to_map;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_STREAM_H_

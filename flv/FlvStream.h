// FlvStream.h

#ifndef _PPBOX_DEMUX_FLV_FLV_STREAM_H_
#define _PPBOX_DEMUX_FLV_FLV_STREAM_H_

#include <ppbox/avformat/flv/FlvFormat.h>
#include <ppbox/avformat/flv/FlvDataType.h>
#include <ppbox/avformat/flv/FlvTagType.h>
#include <ppbox/avformat/codec/aac/AacConfig.h>
using namespace ppbox::avformat;

namespace ppbox
{
    namespace demux
    {

        class FlvStream
            : public ppbox::avformat::FlvTag
            , public StreamInfo
        {
        public:
            FlvStream()
                : ready(false)
            {
                index = (size_t)-1;
            }

            FlvStream(
                FlvTag const & tag, 
                std::vector<boost::uint8_t> const & codec_data,
                FlvMetadata const & metadata)
                : FlvTag(tag)
                , ready(false)
            {
                index = (size_t)-1;
                parse(codec_data, metadata);
            }

        private:
            void parse(
                std::vector<boost::uint8_t> const & codec_data,
                FlvMetadata const & metadata)
            {
                if (DataSize > 0) {
                    if (FlvTagType::VIDEO == Type) { 
                        type = MEDIA_TYPE_VIDE;

                        switch (VideoHeader.CodecID) {
                            case 7:
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = StreamInfo::video_avc_packet;
                                break;
                            default:
                                sub_type = VIDEO_TYPE_NONE;
                                format_type = 0;
                                break;
                        }
                        video_format.frame_rate = metadata.framerate;
                        video_format.width = metadata.width;
                        video_format.height = metadata.height;
                        time_scale = 1000;
                        format_data = codec_data;
                    } else if (FlvTagType::AUDIO == Type) {
                        type = MEDIA_TYPE_AUDI;
                        switch (AudioHeader.SoundFormat) {
                            case 10:
                                sub_type = AUDIO_TYPE_MP4A;
                                format_type = StreamInfo::audio_iso_mp4;
                                break;
                            default:
                                sub_type = AUDIO_TYPE_NONE;
                                format_type = 0;
                                break;
                        }
                        time_scale = 1000;
                        boost::uint32_t frequency[4] = {5500, 11025, 22050, 44100};
                        boost::uint32_t size[2] = {8, 16};
                        boost::uint32_t channel[2] = {1, 2};
                        audio_format.sample_rate = frequency[AudioHeader.SoundRate];
                        audio_format.sample_size = size[AudioHeader.SoundSize];
                        audio_format.channel_count = channel[AudioHeader.SoundType];
                        if (sub_type == AUDIO_TYPE_MP4A) {
                            ppbox::avformat::AacConfig ac(codec_data);
                            audio_format.sample_rate = ac.get_frequency();
                            audio_format.channel_count = ac.channel_configuration;
                        }
                        if (metadata.audiosamplerate > 0)
                            audio_format.sample_rate = metadata.audiosamplerate;
                        if (metadata.audiosamplesize > 0)
                            audio_format.sample_size = metadata.audiosamplesize;
                        format_data = codec_data;
                    }
                }
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_STREAM_H_

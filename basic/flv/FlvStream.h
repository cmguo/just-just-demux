// FlvStream.h

#ifndef _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_
#define _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_

#include <ppbox/avformat/flv/FlvFormat.h>
#include <ppbox/avformat/flv/FlvDataType.h>
#include <ppbox/avformat/flv/FlvTagType.h>
#include <ppbox/avformat/flv/FlvMetaData.h>
#include <ppbox/avformat/codec/avc/AvcConfigHelper.h>
#include <ppbox/avformat/codec/aac/AacConfigHelper.h>

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
                ppbox::avformat::FlvMetaData const & metadata)
                : FlvTag(tag)
                , ready(false)
            {
                index = (size_t)-1;
                parse(codec_data, metadata);
            }

        private:
            void parse(
                std::vector<boost::uint8_t> const & codec_data,
                ppbox::avformat::FlvMetaData const & metadata)
            {
                using namespace ppbox::avformat;

                if (DataSize > 0) {
                    if (FlvTagType::VIDEO == Type) { 
                        type = MEDIA_TYPE_VIDE;
                        time_scale = 1000;
                        video_format.frame_rate = 0;
                        video_format.width = 0;
                        video_format.height = 0;
                        format_data = codec_data;

                        switch (VideoHeader.CodecID) {
                            case FlvVideoCodec::H264:
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = StreamInfo::video_avc_packet;
                                break;
                            default:
                                sub_type = VIDEO_TYPE_NONE;
                                format_type = 0;
                                break;
                        }

                        if (sub_type == VIDEO_TYPE_AVC1) {
                            ppbox::avformat::AvcConfigHelper ac(codec_data);
                            ac.get_format(video_format);
                        }

                        if (metadata.framerate) {
                            video_format.frame_rate = metadata.framerate;
                        }
                        if (metadata.width) {
                            video_format.width = metadata.width;
                        }
                        if (metadata.height) {
                            video_format.height = metadata.height;
                        }
                    } else if (FlvTagType::AUDIO == Type) {
                        type = MEDIA_TYPE_AUDI;
                        time_scale = 1000;
                        boost::uint32_t frequency[4] = {5500, 11025, 22050, 44100};
                        boost::uint32_t size[2] = {8, 16};
                        boost::uint32_t channel[2] = {1, 2};
                        audio_format.sample_rate = frequency[AudioHeader.SoundRate];
                        audio_format.sample_size = size[AudioHeader.SoundSize];
                        audio_format.channel_count = channel[AudioHeader.SoundType];
                        format_data = codec_data;

                        switch (AudioHeader.SoundFormat) {
                            case FlvSoundCodec::AAC:
                                sub_type = AUDIO_TYPE_MP4A;
                                format_type = StreamInfo::audio_iso_mp4;
                                break;
                            case FlvSoundCodec::MP3:
                                sub_type = AUDIO_TYPE_MP1A;
                                format_type = StreamInfo::audio_iso_mp4;
                                format_data.clear();
                                break;
                            default:
                                sub_type = AUDIO_TYPE_NONE;
                                format_type = 0;
                                break;
                        }

                        if (sub_type == AUDIO_TYPE_MP4A) {
                            ppbox::avformat::AacConfigHelper ac(codec_data);
                            audio_format.sample_rate = ac.get_frequency();
                            audio_format.channel_count = ac.get_channel_count();
                        }

                        if (metadata.audiosamplerate > 0)
                            audio_format.sample_rate = metadata.audiosamplerate;
                        if (metadata.audiosamplesize > 0)
                            audio_format.sample_size = metadata.audiosamplesize;
                    }
                }
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_

// FlvStream.h

#ifndef _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_
#define _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_

#include <ppbox/avformat/flv/FlvTagType.h>
#include <ppbox/avformat/flv/FlvEnum.h>
#include <ppbox/avformat/flv/FlvMetaData.h>
#include <ppbox/avformat/Format.h>

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

                if (DataSize == 0) {
                    return;
                }

                time_scale = 1000;

                if (FlvTagType::VIDEO == Type) { 
                    type = StreamType::VIDE;
                    video_format.frame_rate = 0;
                    video_format.width = 0;
                    video_format.height = 0;
                    format_data = codec_data;
                    Format::finish_from_stream(*this, "flv", VideoHeader.CodecID);
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
                    type = StreamType::AUDI;
                    boost::uint32_t frequency[4] = {5500, 11025, 22050, 44100};
                    boost::uint32_t size[2] = {8, 16};
                    boost::uint32_t channel[2] = {1, 2};
                    audio_format.sample_rate = frequency[AudioHeader.SoundRate];
                    audio_format.sample_size = size[AudioHeader.SoundSize];
                    audio_format.channel_count = channel[AudioHeader.SoundType];
                    format_data = codec_data;
                    Format::finish_from_stream(*this, "flv", AudioHeader.SoundFormat);
                    if (metadata.audiosamplerate > 0)
                        audio_format.sample_rate = metadata.audiosamplerate;
                    if (metadata.audiosamplesize > 0)
                        audio_format.sample_size = metadata.audiosamplesize;
                }
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_FLV_FLV_STREAM_H_

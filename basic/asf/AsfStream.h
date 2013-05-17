// AsfStream.h

#ifndef _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_
#define _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <ppbox/avformat/Format.h>

#include <util/archive/ArchiveBuffer.h>

namespace ppbox
{
    namespace demux
    {

        class AsfStream
            : public ppbox::avformat::AsfStreamPropertiesObjectData
            , public StreamInfo
        {
        public:
            AsfStream()
            {
                index = (size_t)-1;
            }

            AsfStream(
                ppbox::avformat::AsfStreamPropertiesObjectData const & property)
                : ppbox::avformat::AsfStreamPropertiesObjectData(property)
            {
                index = (size_t)-1;
                parse();
            }
/*
            void get_start_sample(
                std::vector<Sample> & samples)
            {
                if (sub_type == VIDEO_TYPE_AVC1) {
                    Sample sample;
                    sample.itrack = index_to_map;
                    sample.time = 0;
                    sample.ustime = 0;
                    sample.offset = 0;
                    sample.size = format_data.size();
                    sample.duration = 0;
                    sample.idesc = 0;
                    sample.dts = 0;
                    sample.cts_delta = 0;
                    sample.is_sync = false;
                    sample.is_discontinuity = false;
                    sample.data.swap(format_data);
                    samples.push_back(sample);
                }
            }
*/
        private:
            void parse()
            {
                using namespace ppbox::avformat;

                if (TypeSpecificDataLength > 0) {

                    time_scale = 1000;

                    if (ASF_Video_Media == StreamType) { 
                        type = StreamType::VIDE;
                        video_format.width = Video_Media_Type.EncodeImageWidth;
                        video_format.height = Video_Media_Type.EncodeImageHeight;
                        video_format.frame_rate = 0;
                        format_data = Video_Media_Type.FormatData.CodecSpecificData;
                        Format::finish_stream_info(*this, FormatType::ASF, Video_Media_Type.FormatData.CompressionID);
                    } else if (ASF_Audio_Media == StreamType) {
                        type = StreamType::AUDI;
                        audio_format.channel_count = Audio_Media_Type.NumberOfChannels;
                        audio_format.sample_rate = Audio_Media_Type.SamplesPerSecond;
                        audio_format.sample_size = Audio_Media_Type.BitsPerSample;
                        format_data = Audio_Media_Type.CodecSpecificData;
                        Format::finish_stream_info(*this, FormatType::ASF, Audio_Media_Type.CodecId);
                    }
                }
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_

// AsfStream.h

#ifndef _JUST_DEMUX_BASIC_ASF_ASF_STREAM_H_
#define _JUST_DEMUX_BASIC_ASF_ASF_STREAM_H_

#include <just/avformat/asf/AsfObjectType.h>
#include <just/avformat/Format.h>

#include <util/archive/ArchiveBuffer.h>

namespace just
{
    namespace demux
    {

        class AsfStream
            : public just::avformat::AsfStreamPropertiesObjectData
            , public StreamInfo
        {
        public:
            AsfStream()
            {
                index = (boost::uint32_t)-1;
            }

            AsfStream(
                just::avformat::AsfStreamPropertiesObjectData const & property)
                : just::avformat::AsfStreamPropertiesObjectData(property)
            {
                index = (boost::uint32_t)-1;
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
                using namespace just::avformat;

                if (TypeSpecificDataLength > 0) {

                    time_scale = 1000;
                    boost::system::error_code ec;

                    if (ASF_Video_Media == StreamType) { 
                        type = StreamType::VIDE;
                        video_format.width = Video_Media_Type.EncodeImageWidth;
                        video_format.height = Video_Media_Type.EncodeImageHeight;
                        video_format.frame_rate(0);
                        format_data = Video_Media_Type.FormatData.CodecSpecificData;
                        Format::finish_from_stream(*this, "asf", Video_Media_Type.FormatData.CompressionID, ec);
                    } else if (ASF_Audio_Media == StreamType) {
                        type = StreamType::AUDI;
                        audio_format.channel_count = Audio_Media_Type.NumberOfChannels;
                        audio_format.sample_rate = Audio_Media_Type.SamplesPerSecond;
                        audio_format.sample_size = Audio_Media_Type.BitsPerSample;
                        audio_format.block_align = Audio_Media_Type.BlockAlignment;
                        format_data = Audio_Media_Type.CodecSpecificData;
                        Format::finish_from_stream(*this, "asf", Audio_Media_Type.CodecId, ec);
                    }
                }
            }
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_ASF_ASF_STREAM_H_

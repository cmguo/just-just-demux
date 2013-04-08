// AsfStream.h

#ifndef _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_
#define _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <ppbox/avformat/codec/avc/AvcCodec.h>

#include <util/archive/ArchiveBuffer.h>

namespace ppbox
{
    namespace demux
    {

        class AsfStream
            : public ppbox::avformat::ASF_Stream_Properties_Object_Data
            , public StreamInfo
        {
        public:
            AsfStream()
                : next_id(0)
            {
                index = (size_t)-1;
            }

            AsfStream(
                ppbox::avformat::ASF_Stream_Properties_Object_Data const & property)
                : ppbox::avformat::ASF_Stream_Properties_Object_Data(property)
                , next_id(0)
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
                        type = MEDIA_TYPE_VIDE;

                        video_format.width = Video_Media_Type.EncodeImageWidth;
                        video_format.height = Video_Media_Type.EncodeImageHeight;
                        video_format.frame_rate = 0;
                        format_data = Video_Media_Type.FormatData.CodecSpecificData;

                        switch (Video_Media_Type.FormatData.CompressionID) {
                            case MAKE_FOURC_TYPE('h', '2', '6', '4'):
                            case MAKE_FOURC_TYPE('H', '2', '6', '4'):
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = StreamInfo::video_avc_byte_stream;
                                {
                                    AvcCodec * avc_codec = new AvcCodec(format_data, AvcCodec::from_es_tag());
                                    if (format_data.empty()) {
                                        delete avc_codec;
                                        return;
                                    }
                                    codec = avc_codec;
                                    avc_codec->config_helper().get_format(video_format);
                                }
                                break;
                            default:
                                format_type = 0;
                                sub_type = VIDEO_TYPE_NONE;
                                break;
                        }
                    } else if (ASF_Audio_Media == StreamType) {
                        type = MEDIA_TYPE_AUDI;

                        audio_format.channel_count = Audio_Media_Type.NumberOfChannels;
                        audio_format.sample_rate = Audio_Media_Type.SamplesPerSecond;
                        audio_format.sample_size = Audio_Media_Type.BitsPerSample;
                        format_data = Audio_Media_Type.CodecSpecificData;

                        switch (Audio_Media_Type.CodecId) {
                            case 353:
                                format_type = StreamInfo::audio_microsoft_wave;
                                sub_type = AUDIO_TYPE_WMA2;
                                break;
                            case 255:
                                format_type = StreamInfo::audio_iso_mp4;
                                sub_type = AUDIO_TYPE_MP4A;
                                break;
                            default:
                                format_type = 0;
                                sub_type = AUDIO_TYPE_NONE;
                                break;
                        }
                    }
                }
            }

        public:
            boost::uint32_t next_id;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_ASF_ASF_STREAM_H_

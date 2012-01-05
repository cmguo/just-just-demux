// AsfStream.h

#ifndef _PPBOX_DEMUX_ASF_ASF_STREAM_H_
#define _PPBOX_DEMUX_ASF_ASF_STREAM_H_

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <util/archive/ArchiveBuffer.h>

namespace ppbox
{
    namespace demux
    {

        class AsfStream
            : public ppbox::avformat::ASF_Stream_Properties_Object_Data
            , public MediaInfo
        {
        public:
            AsfStream()
                : ready(false)
                , next_id(0)
                , time_offset_us((boost::uint64_t)-1)
                , time_offset_ms((boost::uint32_t)-1)
            {
                index = (size_t)-1;
            }

            AsfStream(
                ppbox::avformat::ASF_Stream_Properties_Object_Data const & property)
                : ppbox::avformat::ASF_Stream_Properties_Object_Data(property)
                , ready(false)
                , next_id(0)
            {
                index = (size_t)-1;
                time_offset_us = TimeOffset / 10;
                time_offset_ms = (boost::uint32_t)(time_offset_us / 1000);
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
                    if (ASF_Video_Media == StreamType) { 
                        type = MEDIA_TYPE_VIDE;

                        video_format.width = Video_Media_Type.EncodeImageWidth;
                        video_format.height = Video_Media_Type.EncodeImageHeight;
                        video_format.frame_rate = 0;

                        switch (Video_Media_Type.FormatData.CompressionID) {
                            case MAKE_FOURC_TYPE('h', '2', '6', '4'):
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = MediaInfo::video_avc_byte_stream;
                                break;
                            case MAKE_FOURC_TYPE('H', '2', '6', '4'):
                                sub_type = VIDEO_TYPE_AVC1;
                                format_type = MediaInfo::video_avc_byte_stream;
                                break;
                            default:
                                format_type = 0;
                                sub_type = VIDEO_TYPE_NONE;
                                break;
                        }
                        format_data = TypeSpecificData;
                        time_scale = 1000;
                    } else if (ASF_Audio_Media == StreamType) {
                        type = MEDIA_TYPE_AUDI;
                        switch (Audio_Media_Type.CodecId) {
                            case 353:
                                format_type = MediaInfo::audio_microsoft_wave;
                                sub_type = AUDIO_TYPE_WMA2;
                                break;
                            case 255:
                                format_type = MediaInfo::audio_microsoft_wave;
                                sub_type = AUDIO_TYPE_MP4A;
                                break;
                            default:
                                format_type = 0;
                                sub_type = AUDIO_TYPE_NONE;
                                break;
                        }
                        time_scale = 1000;
                        audio_format.channel_count = Audio_Media_Type.NumberOfChannels;
                        audio_format.sample_rate = Audio_Media_Type.SamplesPerSecond;
                        audio_format.sample_size = Audio_Media_Type.BitsPerSample;
                        format_data = TypeSpecificData;
                    }
                }
            }

        public:
            bool ready;
            boost::uint32_t next_id;
            boost::uint64_t time_offset_us;
            boost::uint32_t time_offset_ms;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_ASF_ASF_STREAM_H_

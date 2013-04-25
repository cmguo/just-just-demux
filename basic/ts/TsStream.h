// TsStream.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

#include <ppbox/avformat/codec/avc/AvcCodec.h>
#include <ppbox/avformat/codec/aac/AacCodec.h>
#include <ppbox/avformat/codec/aac/AacConfig.h>
#include <ppbox/avformat/Format.h>

namespace ppbox
{
    namespace demux
    {

        class TsStream
            : public ppbox::avformat::PmtStream
            , public StreamInfo
        {
        public:
            TsStream()
                : ready(false)
            {
                index = (size_t)-1;
            }

            TsStream(
                ppbox::avformat::PmtStream const & info)
                : ppbox::avformat::PmtStream(info)
                , ready(false)
            {
                index = (size_t)-1;
                //time_offset_us = TimeOffset / 10;
                parse();
            }

            ~TsStream()
            {
            }

        public:
            void set_pes(
                std::vector<boost::uint8_t> const & data)
            {
                using namespace ppbox::avformat;

                switch (stream_type) {
                    case TsStreamType::iso_11172_audio:
                    case TsStreamType::iso_13818_3_audio:
                        break;
                    case TsStreamType::iso_13818_7_audio:
                        {
                            AacCodec * aac_codec = new AacCodec(format_type, data);
                            aac_codec->config_helper().to_data(format_data);
                            codec = aac_codec;
                            audio_format.channel_count = aac_codec->config_helper().get_channel_count();
                            audio_format.sample_size = 0;
                            audio_format.sample_rate = aac_codec->config_helper().get_frequency();
                        }
                        break;
                    case TsStreamType::iso_13818_video:
                        {
                            AvcCodec * avc_codec = new AvcCodec(format_type, data);
                            avc_codec->config_helper().to_es_data(format_data);
                            if (format_data.empty()) {
                                delete avc_codec;
                                return;
                            }
                            codec = avc_codec;
                            avc_codec->config_helper().get_format(video_format);
                       }
                        break;
                    default:
                        break;
                }
                time_scale = TsPacket::TIME_SCALE;
                ready = true;
            }

            void clear()
            {
                codec.reset();
                ready = false;
            }

        private:
            void parse()
            {
                using namespace ppbox::avformat;

                switch (stream_type) {
                    case TsStreamType::iso_11172_audio:
                    case TsStreamType::iso_13818_3_audio:
                        type = StreamType::AUDI;
                        sub_type = AudioSubType::MP1A;
                        format_type = FormatType::audio_raw;
                        break;
                    case TsStreamType::iso_13818_7_audio:
                        type = StreamType::AUDI;
                        sub_type = AudioSubType::MP4A;
                        format_type = FormatType::audio_adts;
                        break;
                    case TsStreamType::iso_13818_video:
                        type = StreamType::VIDE;
                        sub_type = VideoSubType::AVC1;
                        format_type =FormatType:: video_avc_byte_stream;
                        break;
                    default:
                        type = StreamType::NONE;
                        format_type =FormatType:: none;
                        time_scale = TsPacket::TIME_SCALE;
                        ready = true;
                        break;
                }
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

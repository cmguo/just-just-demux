// TsStream.h

#ifndef _PPBOX_DEMUX_TS_TS_STREAM_H_
#define _PPBOX_DEMUX_TS_TS_STREAM_H_

#include <ppbox/avformat/codec/avc/AvcCodec.h>
#include <ppbox/avformat/codec/aac/AacCodec.h>

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
                if (codec)
                    delete codec;
            }

        public:
            void set_pes(
                std::vector<boost::uint8_t> const & data)
            {
                using namespace ppbox::avformat;

                switch (stream_type) {
                    case TsStreamType::iso_11172_audio:
                        break;
                    case TsStreamType::iso_13818_7_audio:
                        {
                            AacCodec * aac_codec = new AacCodec(data, AacCodec::from_adts_tag());
                            aac_codec->config_helper().to_data(format_data);
                            codec = aac_codec;
                        }
                        break;
                    case TsStreamType::iso_13818_video:
                        {
                            AvcCodec * avc_codec = new AvcCodec(data, AvcCodec::from_es_tag());
                            avc_codec->config_helper().to_es_data(format_data);
                            codec = avc_codec;
                        }
                        break;
                    default:
                        return;
                }
                time_scale = TsPacket::TIME_SCALE;
                ready = true;
            }

        private:
            void parse()
            {
                using namespace ppbox::avformat;

                switch (stream_type) {
                    case TsStreamType::iso_11172_audio:
                        type = MEDIA_TYPE_AUDI;
                        sub_type = AUDIO_TYPE_MP1A;
                        format_type = audio_iso_mp4;
                        break;
                    case TsStreamType::iso_13818_7_audio:
                        type = MEDIA_TYPE_AUDI;
                        sub_type = AUDIO_TYPE_MP4A;
                        format_type = audio_iso_mp4;
                        break;
                    case TsStreamType::iso_13818_video:
                        type = MEDIA_TYPE_VIDE;
                        sub_type = VIDEO_TYPE_AVC1;
                        format_type = video_avc_byte_stream;
                        break;
                    default:
                        break;
                }
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_TS_TS_STREAM_H_

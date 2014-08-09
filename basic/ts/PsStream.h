// PsStream.h

#ifndef _PPBOX_DEMUX_BASIC_TS_PS_STREAM_H_
#define _PPBOX_DEMUX_BASIC_TS_PS_STREAM_H_

#include <ppbox/avformat/ts/TsFormat.h>
#include <ppbox/avformat/ts/TsEnum.h>

namespace ppbox
{
    namespace demux
    {

        class PsStream
            : public StreamInfo
            , private ppbox::avformat::PsmStream
        {
        public:
            PsStream()
                : ready(false)
            {
            }

            PsStream(
                ppbox::avformat::PsSystemHeader::Stream const & info)
                : ready(false)
            {
                using namespace ppbox::avformat;

                if (info.stream_id == TsStreamId::audio_base)
                    stream_type = TsStreamType::iso_11172_audio;
                else if (info.stream_id == TsStreamId::video_base)
                    stream_type = TsStreamType::iso_13818_2_video;
            }

            ~PsStream()
            {
            }

        public:
            void set(
                ppbox::avformat::PsmStream const & info)
            {
                ppbox::avformat::TsContext c = {0, 0, 0};
                context_ = c;
                context = &context_;

                (ppbox::avformat::PsmStream &)(*this) = info;

                // search for registration_descriptor, descriptor_tag = 5
                {
                    boost::uint32_t format_identifier = 0;
                    for (size_t i = 0; i < descriptor.size(); ++i) {
                        if (descriptor[i].descriptor_tag == 5) {
                            memcpy(&format_identifier, &info.descriptor[i].descriptor.front(), 4);
                            context_.regd_type = format_identifier;
                        }
                    }
                    if (format_identifier == MAKE_FOURC_TYPE('H', 'D', 'M', 'V')
                        || format_identifier == MAKE_FOURC_TYPE('H', 'D', 'P', 'R')) {
                            context_.hdmv_type = stream_type;
                    }
                }
            }

            void set_pes(
                std::vector<boost::uint8_t> const & data)
            {
                using namespace ppbox::avformat;

                time_scale = PsPacket::TIME_SCALE;
                format_data = data;
                boost::system::error_code ec;
                ready = Format::finish_from_stream(*this, "ts", stream_type, ec);
            }

            void clear()
            {
                ready = false;
            }

        public:
            bool ready;

        private:
            ppbox::avformat::TsContext context_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_PS_STREAM_H_

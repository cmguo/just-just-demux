// PsStream.h

#ifndef _JUST_DEMUX_BASIC_MP2_PS_STREAM_H_
#define _JUST_DEMUX_BASIC_MP2_PS_STREAM_H_

#include <just/avformat/mp2/Mp2Format.h>
#include <just/avformat/mp2/Mp2Enum.h>

namespace just
{
    namespace demux
    {

        class PsStream
            : public StreamInfo
            , private just::avformat::PsmStream
        {
        public:
            PsStream()
                : ready(false)
            {
            }

            PsStream(
                just::avformat::PsSystemHeader::Stream const & info)
                : ready(false)
            {
                using namespace just::avformat;

                if (info.stream_id == Mp2StreamId::audio_base)
                    stream_type = Mp2StreamType::iso_11172_audio;
                else if (info.stream_id == Mp2StreamId::video_base)
                    stream_type = Mp2StreamType::iso_13818_2_video;
            }

            ~PsStream()
            {
            }

        public:
            void set(
                just::avformat::PsmStream const & info)
            {
                just::avformat::Mp2Context c = {0, 0, 0};
                context_ = c;
                context = &context_;

                (just::avformat::PsmStream &)(*this) = info;

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
                using namespace just::avformat;

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
            just::avformat::Mp2Context context_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_MP2_PS_STREAM_H_

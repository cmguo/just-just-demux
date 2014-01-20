// TsStream.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

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
            }

            ~TsStream()
            {
            }

        public:
            void set_pes(
                std::vector<boost::uint8_t> const & data)
            {
                using namespace ppbox::avformat;

                time_scale = TsPacket::TIME_SCALE;
                format_data = data;
                boost::system::error_code ec;
                if (stream_type > 0x80) {
                    // search for registration_descriptor, descriptor_tag = 5
                    boost::uint32_t format_identifier = 0;
                    for (size_t i = 0; i < descriptor.size(); ++i) {
                        if (descriptor[i].descriptor_tag == 5) {
                            memcpy(&format_identifier, &descriptor[i].descriptor.front(), 4);
                        }
                    }
                    ready = Format::finish_from_stream(*this, "ts", format_identifier, ec);
                } else {
                    ready = Format::finish_from_stream(*this, "ts", stream_type, ec);
                }
            }

            void clear()
            {
                ready = false;
            }

        public:
            bool ready;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

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
                ready = Format::finish_from_stream(*this, "ts", stream_type);
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

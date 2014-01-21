// TsStream.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

#include <ppbox/avformat/ts/TsFormat.h>

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
                index = (boost::uint32_t)-1;
                ppbox::avformat::TsContext c = {0, 0, 0};
                context_ = c;
            }

            TsStream(
                ppbox::avformat::PmtSection const & sec, 
                ppbox::avformat::PmtStream const & info)
                : ppbox::avformat::PmtStream(info)
                , ready(false)
            {
                index = (boost::uint32_t)-1;
                ppbox::avformat::TsContext c = {0, 0, 0};
                context_ = c;
                context = &context_;

                // search for registration_descriptor, descriptor_tag = 5
                {
                    boost::uint32_t format_identifier = 0;
                    for (size_t i = 0; i < sec.descriptor.size(); ++i) {
                        if (sec.descriptor[i].descriptor_tag == 5) {
                            memcpy(&format_identifier, &sec.descriptor[i].descriptor.front(), 4);
                        }
                    }
                    if (format_identifier == MAKE_FOURC_TYPE('H', 'D', 'M', 'V')
                        || format_identifier == MAKE_FOURC_TYPE('H', 'D', 'P', 'R')) {
                            context_.hdmv_type = stream_type;
                    }
                }

                // search for registration_descriptor, descriptor_tag = 5
                {
                    boost::uint32_t format_identifier = 0;
                    for (size_t i = 0; i < descriptor.size(); ++i) {
                        if (descriptor[i].descriptor_tag == 5) {
                            memcpy(&format_identifier, &descriptor[i].descriptor.front(), 4);
                        }
                    }
                    if (format_identifier) {
                        context_.regd_type = format_identifier;
                    }
                }
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

#endif // _PPBOX_DEMUX_BASIC_TS_TS_STREAM_H_

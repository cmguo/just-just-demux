// Demuxer.h

#ifndef _PPBOX_DEMUX_DEMUXER_H_
#define _PPBOX_DEMUX_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/TimestampHelper.h"

#include <ppbox/common/ClassFactory.h>

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        class Demuxer
            : public DemuxerBase
            , public ppbox::common::ClassFactory<
                Demuxer, 
                std::string, 
                Demuxer * (std::basic_streambuf<boost::uint8_t> &)
            >
        {
        public:
            Demuxer(
                std::basic_streambuf<boost::uint8_t> & buf);

            virtual ~Demuxer();

        public:
            void demux_begin(
                TimestampHelper & helper);

            void demux_end();

        protected:
            void on_open();

            void adjust_timestamp(
                Sample & sample)
            {
                helper_->adjust(sample);
            }

        private:
            TimestampHelper * helper_;
            TimestampHelper default_helper_;
        };

    } // namespace demux
} // namespace ppbox

#define PPBOX_REGISTER_DEMUXER(k, c) PPBOX_REGISTER_CLASS(k, c)

#endif // _PPBOX_DEMUX_DEMUXER_H_

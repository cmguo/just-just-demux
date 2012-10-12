// Demuxer.h

#ifndef _PPBOX_DEMUX_DEMUXER_H_
#define _PPBOX_DEMUX_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/TimestampHelper.h"

#include <ppbox/common/Call.h>
#include <ppbox/common/Create.h>

#include <util/smart_ptr/RefenceFromThis.h>

#define PPBOX_REGISTER_DEMUXER(n, c) \
    static ppbox::common::Call reg ## n(ppbox::demux::Demuxer::register_demuxer, BOOST_PP_STRINGIZE(n), ppbox::common::Creator<c>())

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        class Demuxer
            : public DemuxerBase
        {
        public:
            typedef boost::function<Demuxer * (
                std::basic_streambuf<boost::uint8_t> & buf)
            > register_type;

        public:
            Demuxer(
                std::basic_streambuf<boost::uint8_t> & buf);

            virtual ~Demuxer();

        public:
            static Demuxer * create(
                std::string const & format, 
                std::basic_streambuf<boost::uint8_t> & buf);

            static void register_demuxer(
                std::string const & format,
                register_type func);

            static void destory(
                Demuxer *& demuxer);

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

        private:
            static std::map<std::string, register_type> & demuxer_map();
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_H_

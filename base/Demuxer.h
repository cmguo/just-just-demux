// Demuxer.h

#ifndef _PPBOX_DEMUX_DEMUXER_H_
#define _PPBOX_DEMUX_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/DemuxStatistic.h"
#include "ppbox/demux/base/TimestampHelper.h"

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        class Demuxer
            : public DemuxerBase
            , public DemuxStatistic
        {
        public:
            Demuxer(
                boost::asio::io_service & io_svc);

            virtual ~Demuxer();

        public:
            virtual boost::system::error_code open (
                boost::system::error_code & ec);

            virtual void async_open(
                open_response_type const & resp);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code get_media_info(
                MediaInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

            virtual bool get_data_stat(
                DataStatistic & stat, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual boost::uint64_t check_seek(
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

            virtual bool fill_data(
                boost::system::error_code & ec);

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

            TimestampHelper & time_helper()
            {
                return *helper_;
            }

        private:
            TimestampHelper * helper_;
            TimestampHelper default_helper_;
        };

    } // namespace demux
} // namespace ppbox

#define PPBOX_REGISTER_BASIC_DEMUXER(k, c) PPBOX_REGISTER_CLASS(k, c)

#endif // _PPBOX_DEMUX_DEMUXER_H_

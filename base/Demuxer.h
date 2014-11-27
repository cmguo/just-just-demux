// Demuxer.h

#ifndef _JUST_DEMUX_DEMUXER_H_
#define _JUST_DEMUX_DEMUXER_H_

#include "just/demux/base/DemuxerBase.h"
#include "just/demux/base/DemuxStatistic.h"
#include "just/demux/base/TimestampHelper.h"

namespace just
{
    namespace demux
    {

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
                DataStat & stat, 
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

        protected:
            void on_open();

            void on_close();

            void adjust_timestamp(
                Sample & sample)
            {
                timestamp_->adjust(sample);
            }

            void timestamp(
                TimestampHelper & timestamp)
            {
                timestamp_ = &timestamp;
            }

            TimestampHelper & timestamp()
            {
                return *timestamp_;
            }

            TimestampHelper const & timestamp() const
            {
                return *timestamp_;
            }

            TimestampHelper & default_timestamp()
            {
                return default_timestamp_;
            }

        private:
            TimestampHelper * timestamp_;
            TimestampHelper default_timestamp_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_DEMUXER_H_

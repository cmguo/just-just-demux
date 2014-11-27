// CustomDemuxer.h

#ifndef _JUST_DEMUX_BASE_CUSTOM_DEMUXER_H_
#define _JUST_DEMUX_BASE_CUSTOM_DEMUXER_H_

#include "just/demux/base/Demuxer.h"

namespace just
{
    namespace demux
    {

        class CustomDemuxer
            : public Demuxer
        {
        public:
            CustomDemuxer(
                boost::asio::io_service & io_svc);

            CustomDemuxer(
                DemuxerBase & demuxer);

            virtual ~CustomDemuxer();

        public:
            virtual boost::system::error_code open (
                boost::system::error_code & ec);

            virtual void async_open(
                open_response_type const & resp);

            virtual bool is_open(
                boost::system::error_code & ec);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code get_media_info(
                just::data::MediaInfo & info,
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec) const;

            virtual bool get_data_stat(
                DataStat & stat, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        protected:
            void attach(DemuxerBase & demuxer)
            {
                assert(demuxer_ == NULL);
                demuxer_ = &demuxer;
            }

            DemuxerBase & detach()
            {
                DemuxerBase * demuxer = demuxer_;
                demuxer_ = NULL;
                return *demuxer;
            }

        private:
            DemuxerBase * demuxer_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASE_CUSTOM_DEMUXER_H_

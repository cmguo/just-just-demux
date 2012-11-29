// CustomDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_CUSTOM_DEMUXER_H_
#define _PPBOX_DEMUX_BASE_CUSTOM_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"

namespace ppbox
{
    namespace demux
    {

        class CustomDemuxer
            : public DemuxerBase
        {
        public:
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
                ppbox::data::MediaInfo & info,
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                ppbox::avformat::StreamInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec) const;

            virtual bool get_data_stat(
                DataStatistic & stat, 
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
                ppbox::avformat::Sample & sample, 
                boost::system::error_code & ec);

        private:
            DemuxerBase & demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_CUSTOM_DEMUXER_H_

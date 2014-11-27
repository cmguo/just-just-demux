// SingleDemuxer.h

#ifndef _JUST_DEMUX_BASE_SINGLE_SINGLE_DEMUXER_H_
#define _JUST_DEMUX_BASE_SINGLE_SINGLE_DEMUXER_H_

#include "just/demux/base/CustomDemuxer.h"

#include <just/data/base/MediaBase.h>

#include <util/event/Event.h>

#include <framework/timer/Ticker.h>

namespace just
{
    namespace data
    {
        class UrlSource;
        class SingleSource;
        class SingleBuffer;
    }

    namespace demux
    {

        class SingleDemuxer
            : public CustomDemuxer
        {
        public:
            enum StateEnum
            {
                closed,
                media_open,
                demuxer_probe,
                demuxer_open,
                opened,
            };

        public:
            SingleDemuxer(
                boost::asio::io_service & io_svc, 
                just::data::MediaBase & media);

            virtual ~SingleDemuxer();

        public:
            boost::system::error_code open (
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

        public:
            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual boost::uint64_t check_seek(
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

        public:
            virtual bool fill_data(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual bool free_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

            virtual bool get_data_stat(
                DataStat & stat, 
                boost::system::error_code & ec) const;

        public:
            just::data::MediaBase const & media() const
            {
                return media_;
            }

            just::data::SingleSource const & source() const
            {
                return *source_;
            }

        private:
            bool create_demuxer(
                boost::system::error_code & ec);

            bool is_open(
                boost::system::error_code & ec) const;

            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

        private:
            just::data::MediaBase & media_;
            just::data::SingleSource * source_;
            just::data::SingleBuffer * stream_;

            framework::string::Url url_;
            just::data::MediaInfo media_info_;

            boost::uint64_t seek_time_;
            bool seek_pending_;

            StateEnum open_state_;
            open_response_type resp_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASE_SINGLE_SINGLE_DEMUXER_H_

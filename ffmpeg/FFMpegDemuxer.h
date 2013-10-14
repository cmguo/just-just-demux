// FFMpegDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_DEMUXER_H_
#define _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_DEMUXER_H_

#include "ppbox/demux/base/Demuxer.h"

#include <ppbox/data/base/MediaBase.h>

#include <util/event/Event.h>

#include <framework/timer/Ticker.h>
#include <framework/memory/SmallFixedPool.h>

struct AVFormatContext;
struct AVPacket;

namespace ppbox
{
    namespace data
    {
        class UrlSource;
        class SingleSource;
        class SingleBuffer;
    }

    namespace demux
    {

        class FFMpegDemuxer
            : public Demuxer
        {
        public:
            enum StateEnum
            {
                closed,
                media_open,
                demuxer_open,
                opened,
            };

        public:
            FFMpegDemuxer(
                boost::asio::io_service & io_svc, 
                ppbox::data::MediaBase & media);

            virtual ~FFMpegDemuxer();

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
                MediaInfo & info,
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
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
            ppbox::data::MediaBase const & media() const
            {
                return media_;
            }

            ppbox::data::SingleSource const & source() const
            {
                return *source_;
            }

        private:
            bool avformat_open(
                boost::system::error_code & ec);

            bool is_open(
                boost::system::error_code & ec) const;

            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            void free_packet(
                ppbox::data::MemoryLock * lock);

        private:
            ppbox::data::MediaBase & media_;
            ppbox::data::SingleSource * source_;
            ppbox::data::SingleBuffer * buffer_;
            framework::memory::SmallFixedPool mem_lock_pool_;
            AVFormatContext * avf_ctx_;
            std::deque<AVPacket *> peek_packets_;
            boost::uint64_t start_time_; // AV_TIME_BASE units

            framework::string::Url url_;
            MediaInfo media_info_;
            std::vector<StreamInfo> streams_;

            boost::uint64_t seek_time_;
            bool seek_pending_;

            StateEnum open_state_;
            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_DEMUXER_H_

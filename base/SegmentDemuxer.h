// SegmentDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_

#include "ppbox/demux/base/DemuxError.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/DemuxStatistic.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/DemuxStrategy.h"

#include <ppbox/data/MediaBase.h>
#include <ppbox/data/SourceBase.h>

#include <util/event/Event.h>

#include <framework/timer/Ticker.h>

#include <boost/function.hpp>

namespace ppbox
{
    namespace data
    {
        class SegmentSource;
    }

    namespace demux
    {

        class SegmentBuffer;
        class BytesStream;

        struct DemuxerInfo
        {
            SegmentPosition segment;
            DemuxerBase * demuxer;
            bool is_read_stream;
            boost::uint32_t nref;
        };

        class SegmentDemuxer
            : public DemuxStatistic
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

            enum StateEnum
            {
                not_open,
                source_open,
                demuxer_open,
                open_finished,
                canceling,
            };

        public:
            SegmentDemuxer(
                boost::asio::io_service & io_svc, 
                ppbox::data::MediaBase & media);

            virtual ~SegmentDemuxer();

        public:
            virtual boost::system::error_code open (
                boost::system::error_code & ec);

            virtual void async_open(
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

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

        public:
            boost::system::error_code get_media_info(
                ppbox::data::MediaInfo & info,
                boost::system::error_code & ec);

            size_t get_stream_count(
                boost::system::error_code & ec);

            boost::system::error_code get_stream_info(
                size_t index, 
                ppbox::avformat::StreamInfo & info, 
                boost::system::error_code & ec);

        public:
            boost::uint64_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint64_t get_end_time(
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                ppbox::avformat::Sample & sample, 
                boost::system::error_code & ec);

        public:
            boost::system::error_code get_sample_buffered(
                ppbox::avformat::Sample & sample, 
                boost::system::error_code & ec);

            boost::uint64_t get_buffer_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

        public:
            SegmentBuffer const & buffer() const
            {
                return *buffer_;
            }

            ppbox::data::SegmentSource const & source() const
            {
                return *source_;
            }

        public:
            virtual void on_event(
                util::event::Event const & event);

        protected:
            void tick_on();

        protected:
            typedef boost::function<
                void(void)> event_func;

            typedef boost::function<
                void (event_func const &)> post_event_func;

            post_event_func get_poster();

        private:
            struct EventQueue
            {
                boost::mutex mutex;
                std::vector<event_func> events;
            };

            static void post_event(
                boost::shared_ptr<EventQueue> const & events, 
                event_func const & event);

            void handle_events();

        private:
            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            DemuxerInfo * alloc_demuxer(
                SegmentPosition const & segment, 
                bool read, 
                boost::system::error_code & ec);

            void free_demuxer(
                DemuxerInfo * info, 
                boost::system::error_code & ec);

            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;

        private:
            ppbox::data::MediaBase & media_;
            ppbox::data::SegmentSource * source_;
            SegmentBuffer * buffer_;
            DemuxStrategy * root_content_;

            ppbox::data::MediaInfo media_info_;
            std::vector<ppbox::avformat::StreamInfo> stream_infos_;
            TimestampHelper timestamp_helper_;

            DemuxerInfo * read_demuxer_;
            DemuxerInfo * write_demuxer_;
            std::vector<DemuxerInfo *> demuxer_infos_;
            boost::uint32_t max_demuxer_infos_;

            framework::timer::Ticker * ticker_;
            boost::uint64_t seek_time_;

            StateEnum open_state_;
            SegmentPosition open_segment_;
            open_response_type resp_;

        private:
            boost::shared_ptr<EventQueue> events_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BUFFER_DEMUXER_H_

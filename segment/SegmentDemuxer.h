// SegmentDemuxer.h

#ifndef _PPBOX_DEMUX_SEGMENT_SEGMENT_DEMUXER_H_
#define _PPBOX_DEMUX_SEGMENT_SEGMENT_DEMUXER_H_

#include "ppbox/demux/base/DemuxError.h"
#include "ppbox/demux/base/Demuxer.h"
#include "ppbox/demux/base/DemuxStatistic.h"
#include "ppbox/demux/basic/JointContext.h"

#include <ppbox/data/segment/SegmentMedia.h>

#include <util/event/Event.h>

#include <framework/timer/Ticker.h>

namespace ppbox
{
    namespace data
    {
        class SegmentSource;
        class SegmentBuffer;
        struct SegmentPosition;
        class SegmentStream;
    }

    namespace demux
    {

        class DemuxStrategy;
        struct DemuxerInfo;

        class SegmentDemuxer
            : public Demuxer
        {
        public:
            enum StateEnum
            {
                not_open,
                media_open,
                demuxer_open,
                open_finished,
            };

        public:
            SegmentDemuxer(
                boost::asio::io_service & io_svc, 
                ppbox::data::SegmentMedia & media);

            virtual ~SegmentDemuxer();

        public:
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

        public:
            virtual bool fill_data(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_sample(
                ppbox::avformat::Sample & sample, 
                boost::system::error_code & ec);

            virtual bool free_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

            virtual bool get_data_stat(
                DataStatistic & stat, 
                boost::system::error_code & ec) const;

        public:
            ppbox::data::SegmentMedia const & media() const
            {
                return media_;
            }

            ppbox::data::SegmentSource const & source() const
            {
                return *source_;
            }

            ppbox::data::SegmentBuffer const & buffer() const
            {
                return *buffer_;
            }

        public:
            virtual void on_event(
                util::event::Event const & event);

        protected:
            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec);

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

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
            bool is_open(
                boost::system::error_code & ec) const;

            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            DemuxerInfo * alloc_demuxer(
                ppbox::data::SegmentPosition const & segment, 
                bool is_read, 
                boost::system::error_code & ec);

            void free_demuxer(
                DemuxerInfo * info, 
                bool is_read, 
                boost::system::error_code & ec);

        private:
            ppbox::data::SegmentMedia & media_;
            DemuxStrategy * strategy_;
            ppbox::data::SegmentSource * source_;
            ppbox::data::SegmentBuffer * buffer_;

        private:
            // config
            boost::uint32_t source_time_out_; // 5 seconds
            boost::uint32_t buffer_capacity_; // 10M
            boost::uint32_t buffer_read_size_; // 10K

        private:
            ppbox::data::MediaInfo media_info_;
            std::vector<ppbox::avformat::StreamInfo> stream_infos_;
            JointContext joint_context_;

            DemuxerInfo * read_demuxer_;
            DemuxerInfo * write_demuxer_;
            std::vector<DemuxerInfo *> demuxer_infos_;
            boost::uint32_t max_demuxer_infos_;

            framework::timer::Ticker * ticker_;
            boost::uint64_t seek_time_;
            bool seek_pending_;

            StateEnum open_state_;
            open_response_type resp_;

        private:
            boost::shared_ptr<EventQueue> events_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENT_SEGMENT_DEMUXER_H_

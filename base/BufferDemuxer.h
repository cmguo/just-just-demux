// BufferDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_

#include "ppbox/demux/base/DemuxerError.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/DemuxerStatistic.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceBase.h"

#include <framework/timer/Ticker.h>

#include <boost/function.hpp>

namespace ppbox
{
    namespace demux
    {
        enum _eEvtType
        {
            INS_NONE = -1,
            INS_TIME = 0,
            INS_BEG,
            INS_END,
        };

        struct InsertMediaInfo
        {
            InsertMediaInfo()
                : id( -1 )
                , insert_time( ~0 )
                , media_duration( ~0 )
                , media_size( ~0 )
                , head_size( ~0 )
                , report( 0 )
                , url( "" )
                , report_begin_url( "" )
                , report_end_url( "" )
                , event_time( 0 )
                , event_type( INS_NONE )
                , argment( 0 ) {}

            InsertMediaInfo(
                boost::uint32_t id, boost::uint64_t insert_time, boost::uint64_t media_duration,
                boost::uint64_t media_size, boost::uint64_t head_size, std::string const & url )
                : id( id )
                , insert_time( insert_time )
                , media_duration( media_duration )
                , media_size( media_size )
                , head_size( head_size )
                , report( 0 )
                , url( url )
                , report_begin_url( "" )
                , report_end_url( "" )
                , event_time( 0 )
                , event_type( INS_NONE )
                , argment( 0 ) {}

            boost::uint32_t id;
            boost::uint32_t insert_time;      // 插入的时间点
            boost::uint64_t media_duration;   // 影片时长
            boost::uint64_t media_size;       // 影片大小
            boost::uint64_t head_size;        // 文件头部大小
            boost::uint32_t report;
            std::string url;                 // 影片URL
            std::string report_begin_url;
            std::string report_end_url;
            // ret
            boost::uint64_t event_time;
            boost::uint16_t event_type;
            boost::uint32_t argment;
        };

        template <typename Archive>
        void serialize(Archive & ar, InsertMediaInfo & t)
        {
            ar & t.id;
            ar & t.insert_time;
            ar & t.media_duration;
            ar & t.media_size;
            ar & t.head_size;
            ar & t.report;
            ar & t.url;
            ar & t.report_begin_url;
            ar & t.report_end_url;
            ar & t.event_time;
            ar & t.event_type;
            ar & t.argment;
        }

        class BufferList;
        class BytesStream;

        struct DemuxerInfo;

        class BufferDemuxer
            : public DemuxerStatistic
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            BufferDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size,
                SourceBase * source);

            virtual ~BufferDemuxer();

        public:
            virtual boost::system::error_code open (
                std::string const & name, 
                boost::system::error_code & ec);

            virtual void async_open(
                std::string const & name, 
                open_response_type const & resp);

            virtual boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

        public:
            virtual void on_error(
                boost::system::error_code & ec);

        public:
            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            boost::uint32_t get_duration(
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec);

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec);

        public:
            boost::system::error_code get_sample_buffered(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::uint32_t get_buffer_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            void segment_write_beg(
                SegmentPositionEx & segment);

            void on_extern_error(
                boost::system::error_code const & ec);

        public:
            BufferStatistic const & buffer_stat() const
            {
                return (BufferStatistic &)(*buffer_);
            }

        protected:
            boost::system::error_code insert_source(
                boost::uint32_t & time,
                SourceBase * source, 
                boost::system::error_code & ec);

            boost::system::error_code remove_source(
                SourceBase * source, 
                boost::system::error_code & ec);

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
            void handle_async(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            void create_demuxer(
                SegmentPositionEx const & segment, 
                DemuxerInfo & demuxer, 
                boost::system::error_code & ec);

            void reload_demuxer(
                DemuxerPointer & demuxer, 
                SegmentPositionEx & segment, 
                DemuxerInfo & demuxer_info, 
                boost::uint32_t time, 
                bool is_seek, 
                boost::system::error_code & ec);

            void change_source(
                SegmentPositionEx & new_segment, 
                DemuxerInfo & demuxer,
                bool is_seek, 
                boost::system::error_code & ec);

            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;
            boost::system::error_code extern_error_;
            SourceBase * root_source_;
            BufferList * buffer_;

        private:
            framework::timer::Ticker * ticker_;
            boost::uint32_t seek_time_;

            boost::uint64_t segment_time_;
            boost::uint64_t segment_ustime_;

            DemuxerInfo read_demuxer_;
            DemuxerInfo write_demuxer_;

            std::vector<boost::uint32_t> media_time_scales_;
            std::vector<boost::uint64_t> dts_offset_;

            open_response_type resp_;

        private:
            boost::shared_ptr<EventQueue> events_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BUFFER_DEMUXER_H_

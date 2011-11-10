// SegmentsBase.h

#ifndef _PPBOX_DEMUX_SEGMENTS_BASE_H_
#define _PPBOX_DEMUX_SEGMENTS_BASE_H_

#include "ppbox/demux/DemuxerType.h"

#include <boost/filesystem/path.hpp>
#include <boost/asio/buffer.hpp>

#include <framework/network/NetName.h>
#include <framework/string/Url.h>

#include <util/buffers/Buffers.h>
#include <util/protocol/http/HttpError.h>
#include <util/protocol/http/HttpClient.h>

namespace ppbox
{
    namespace demux
    {

        struct Segment
        {
            Segment()
                : begin(0)
                , head_length(0)
                , file_length(0)
                , duration(0)
                , duration_offset(0)
                , duration_offset_us(0)
                , total_state(not_init)
                , num_try(0)
                , demuxer_type(DemuxerType::mp4)
            {
            }

            Segment(
                DemuxerType::Enum demuxer_type)
                : begin(0)
                , head_length(0)
                , file_length(0)
                , duration(0)
                , duration_offset(0)
                , duration_offset_us(0)
                , total_state(not_init)
                , num_try(0)
                , demuxer_type(demuxer_type)
            {
            }

            enum TotalStateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 
            };

            boost::uint64_t begin;
            boost::uint64_t head_length;
            boost::uint64_t file_length;
            boost::uint32_t duration;   // 分段时长（毫秒）
            boost::uint32_t duration_offset;    // 相对起始的时长起点，（毫秒）
            boost::uint64_t duration_offset_us; // 同上，（微秒）
            TotalStateEnum total_state;
            size_t num_try;
            DemuxerType::Enum demuxer_type;
        };

        class Segments
        {
        public:
            Segments()
                : num_del_(0)
                , segment_()
            {
            }

            Segment & operator[](
                boost::uint32_t index)
            {
                if (index < num_del_)
                    return segment_;

                return segments_[index - num_del_];
            }

            Segment const & operator[](
                boost::uint32_t index) const
            {
                if (index < num_del_)
                    return segment_;

                return segments_[index - num_del_];
            }

            size_t size() const
            {
                return segments_.size() + num_del_;
            }

            void push_back(
                const Segment & segment)
            {
                segments_.push_back(segment);
            }

            void pop_front()
            {
                if (segments_.size() >= 2) {
                    segments_[1].file_length += segments_[0].file_length;
                    ++num_del_;
                    segments_.pop_front();
                }
            }

            size_t num_del() const
            {
                return num_del_;
            }

        private:
            size_t num_del_;
            std::deque<Segment> segments_;
            Segment segment_;
        };

        class SegmentsBase
        {
        protected:
            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

            typedef boost::function<void(
                boost::system::error_code const &,
                size_t)
            > read_handle_type;

        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            SegmentsBase(
                boost::asio::io_service & io_svc, 
                boost::uint16_t port)
                : next_source_(NULL)
            {
            }

            virtual ~SegmentsBase()
            {
            }

        public:
            void set_next_source(
                SegmentsBase & source)
            {
                next_source_ = & source;
            }

            SegmentsBase * get_next_source()
            {
                return next_source_;
            }

        public:
            virtual boost::system::error_code segment_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec) = 0;

            virtual void segment_async_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp) = 0;

            virtual bool segment_is_open(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t total(
                boost::system::error_code & ec) = 0;

            virtual std::size_t segment_read(
                write_buffer_t const & buffers,
                boost::system::error_code & ec) = 0;

            virtual void segment_async_read(
                write_buffer_t const & buffers,
                read_handle_type handler) = 0;

            virtual bool continuable(
                boost::system::error_code const & ec) = 0;

            virtual bool recoverable(
                boost::system::error_code const & ec) = 0;

            virtual boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec) = 0;

        public:
            virtual void on_error(
                boost::system::error_code & ec) {}

            virtual void on_seg_beg(
                size_t segment) {}

            virtual void on_seg_end(
                size_t segment) {}

            virtual void on_seg_close(
                size_t segment) {}

        public:
            virtual Segment & operator [](
                size_t segment) = 0;

            virtual Segment const & operator [](
                size_t segment) const = 0;

            virtual size_t total_segments() const = 0;

        private:
            SegmentsBase * next_source_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENTS_BASE_H_
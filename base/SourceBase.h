// SourceBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_BASE_H_
#define _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/SourceTreeItem.h"

#include <util/buffers/Buffers.h>

#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        class SourceTreeItem;

        struct SegmentPosition
            : SourceTreePosition
        {
            SegmentPosition()
                : segment(0)
                , total_state(not_init)
                , size_beg(0)
                , size_end((boost::uint64_t)-1)
                , time_beg(0)
                , time_end((boost::uint64_t)-1)
            {
            }

            friend bool operator == (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition)l == (SourceTreePosition)r
                    && l.segment == r.segment;
            }

            friend bool operator != (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition)l != (SourceTreePosition)r
                    || l.segment != r.segment;
            }

            enum TotalStateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 
            };

            size_t segment;
            TotalStateEnum total_state;
            boost::uint64_t size_beg; // 全局的偏移
            boost::uint64_t size_end; // 全局的偏移
            boost::uint64_t time_beg; // 全局的偏移
            boost::uint64_t time_end; // 全局的偏移
        };

        typedef boost::intrusive_ptr<
            BytesStream> StreamPointer;

        typedef boost::intrusive_ptr<
            DemuxerBase> DemuxerPointer;

        struct DemuxerInfo
        {
            StreamPointer stream;
            DemuxerPointer demuxer;
        };

        class SourceBase
            : public SourceTreeItem
        {
        public:
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
            SourceBase(
                boost::asio::io_service & io_svc,
                DemuxerType::Enum demuxer_type);

            virtual ~SourceBase();

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

            virtual boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec) = 0;

        public:
            virtual void on_error(
                boost::system::error_code & ec) {}

            virtual void on_seg_beg(
                size_t segment) {}

            virtual void on_seg_end(
                size_t segment) {}

        public:
            virtual void next_segment(
                SegmentPosition & position);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // 微妙
                SegmentPosition & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code size_seek (
                boost::uint64_t size,  
                SegmentPosition & position, 
                boost::system::error_code & ec);

            virtual boost::uint64_t segment_head_size(
                size_t segment)
            {
                return 0;
            }

            DemuxerType::Enum const demuxer_type() const
            {
                return demuxer_type_;
            }

        private:
            virtual size_t segment_count() const = 0;

            virtual boost::uint64_t segment_size(
                size_t segment) = 0;

            virtual boost::uint64_t segment_time(
                size_t segment) = 0;

            // 自己所有分段的size总和
            virtual boost::uint64_t source_size();

            // 
            virtual boost::uint64_t source_size_before(
                size_t segment);

            // 自己所有分段的time总和
            virtual boost::uint64_t source_time();

            virtual boost::uint64_t source_time_before(
                size_t segment);

            // 自己和所有子节点的size总和
            virtual boost::uint64_t tree_size();

            virtual boost::uint64_t tree_size_before(
                SourceBase * child);

            // 自己和所有子节点的time总和
            virtual boost::uint64_t tree_time();

            virtual boost::uint64_t tree_time_before(
                SourceBase * child);

            // 所有节点在child插入点之前的size总和
            virtual boost::uint64_t total_size_before(
                SourceBase * child);

            // 所有节点在child插入点之前的time总和
            virtual boost::uint64_t total_time_before(
                SourceBase * child);

        private:
            DemuxerType::Enum demuxer_type_; 
            size_t insert_segment_; // 插入在父节点的分段
            boost::uint64_t insert_size_; // 插入在分段上的偏移位置，相对于分段起始位置
            boost::uint64_t insert_delta_; // 需要重复下载的数据量
            boost::uint64_t insert_time_; // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

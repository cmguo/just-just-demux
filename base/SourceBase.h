// SourceBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_BASE_H_
#define _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

#include "ppbox/demux/base/SourceTreeItem.h"

namespace ppbox
{
    namespace demux
    {

        class SourceTreeItem;

        struct SegmentPosition
            : SourceTreePosition
        {
            SegmentPosition(
                boost::uint64_t seg_off = 0)
                : next_child(NULL)
                , segment((size_t)-1)
                , seg_beg(0)
                , seg_end((boost::uint64_t)-1)
            {
            }

            SegmentPosition(
                size_t segment,
                boost::uint64_t seg_off = 0)
                : segment(segment)
                , seg_beg(0)
                , seg_end((boost::uint64_t)-1)
            {
            }

            enum TotalStateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 
            };

            TotalStateEnum total_state;
            size_t segment;
            DemuxerType::Enum demuxer_type;
            boost::uint64_t seg_beg; // 全局的偏移
            boost::uint64_t seg_end; // 全局的偏移
            boost::uint64_t time_beg; // 全局的偏移
            boost::uint64_t time_end; // 全局的偏移
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
                boost::asio::io_service & io_svc)
            {
            }

            virtual ~SourceBase()
            {
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

        public:
            virtual void next_segment(
                SegmentPosition & position);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // 微妙
                SegmentPosition & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code offset_seek (
                boost::uint64_t offset,  
                SegmentPosition & position, 
                boost::system::error_code & ec);

        private:
            virtual size_t segment_count() const = 0;

            virtual boost::uint64_t segment_size(
                size_t segment) = 0;

            virtual boost::uint64_t total_size();

            virtual boost::uint64_t total_time();

            virtual boost::system::error_code offset_of_segment(
                boost::uint64_t & offset,
                SegmentPosition & position, 
                boost::system::error_code & ec) const
            {
                return ec = boost::system::error_code();
            }

            virtual boost::system::error_code offset_to_segment(
                boost::uint64_t offset,
                SegmentPosition & position,
                boost::system::error_code & ec) const
            {
                return ec = boost::system::error_code();
            }

        private:
            // Source开始位置相对于所有数据的位置
            boost::uint64_t offset(
                SegmentPosition & position);

            // 自己和所有子节点的size总和
            boost::uint64_t total_tree_size();

            boost::uint64_t size_before(
                SourceTreeItem * child);

        private:
            size_t insert_segment_; // 插入在父节点的分段
            boost::uint64_t insert_offset_; // 插入在分段上的偏移位置，相对于分段起始位置
            boost::uint64_t insert_delta_; // 需要重复下载的数据量
            boost::uint64_t insert_time_; // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

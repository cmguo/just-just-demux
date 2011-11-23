// SegmentsBase.h

#ifndef _PPBOX_DEMUX_SEGMENTS_BASE_H_
#define _PPBOX_DEMUX_SEGMENTS_BASE_H_

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
                , total_state(not_init)
                , num_try(0)
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
            TotalStateEnum total_state;
            size_t num_try;
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

        class SourceTreeItem;

        struct SegmentPosition
        {
            SegmentPosition(
                boost::uint64_t seg_off = 0)
                : next_child(NULL)
                , segment((size_t)-1)
                , seg_off(seg_off)
                , seg_beg(0)
                , seg_end((boost::uint64_t)-1)
            {
            }

            SegmentPosition(
                size_t segment,
                boost::uint64_t seg_off = 0)
                : segment(segment)
                , seg_off(seg_off)
                , seg_beg(0)
                , seg_end((boost::uint64_t)-1)
            {
            }

            SourceTreeItem * next_child;// 处理流程中的下一个子节点
            size_t segment;
            boost::uint64_t seg_off;
            boost::uint64_t seg_beg;
            boost::uint64_t seg_end;
        };

        class SourceBase;

        class SourceTreeItem
        {
        public:
            SourceTreeItem()
                : insert_offset_(0)
                , parent_(NULL)
                , first_child_(NULL)
                , prev_sibling_(NULL)
                , next_sibling_(NULL)
            {
            }

        public:
            bool insert_child(
                boost::uint64_t position, // 相对于自身（被插入节点）的数据起始位置
                SegmentPosition & read_pos,
                boost::uint64_t read_offset,
                SegmentPosition & write_pos,
                boost::uint64_t write_offset,
                SourceTreeItem * child)
            {
                child->parent_ = this;
                if (first_child_) {
                    SourceTreeItem * item = first_child_;
                    while (item) {
                        if (item->insert_offset_ > position) {
                            if (item->prev_sibling_) {
                                item->prev_sibling_->next_sibling_ = child;
                                child->prev_sibling_ = item->prev_sibling_;
                                child->next_sibling_ = item;
                                child->insert_offset_ = position;
                                item->prev_sibling_ = child;
                            } else {
                                first_child_ = child;
                                child->next_sibling_ = item;
                                child->insert_offset_ = position;
                                item->prev_sibling_ = child;
                            }
                            break;
                        } else {
                            item = item->next_sibling_;
                            if (!item) {
                                item->next_sibling_ = child;
                                child->prev_sibling_ = item;
                                child->insert_offset_ = position;
                                break;
                            }
                        }
                    }
                } else {
                    first_child_ = child;
                    first_child_->insert_offset_ = position;
                }

                if (position > write_offset) {
                    if (write_pos.next_child) {
                        if (position < write_pos.next_child->insert_offset_) {
                            write_pos.next_child = child;
                        }
                    } else {
                        write_pos.next_child = child;
                    }
                }
                if (position > read_offset) {
                    if (read_pos.next_child) {
                        if (position < read_pos.next_child->insert_offset_) {
                            read_pos.next_child = child;
                        }
                    } else {
                        read_pos.next_child = child;
                    }
                }
                return true;
            }

            bool remove_child(
                SourceBase * child,
                SegmentPosition & read_pos,
                SegmentPosition & write_pos)
            {
                return remove_child(child, read_pos, write_pos, true);
            }

            SourceBase * next_source(
                SegmentPosition & position) const // 返回下一个Source
            {
                if (position.next_child) {
                    SourceTreeItem * next = position.next_child;
                    position.next_child = next->first_child_;
                    position.seg_off = next->offset(position);
                    return (SourceBase *)next;
                } else {
                    position.next_child = next_sibling_;
                    return (SourceBase *)parent_;
                }
            }

            bool remove_self(
                SegmentPosition & read_pos,
                SegmentPosition & write_pos)
            {
                return parent_->remove_child((SourceBase *) this, read_pos, write_pos);
            }

            boost::uint64_t begin(
                SegmentPosition & position) const
            {
                /*if (position.next_child) {
                    return position.next_child->insert_offset_;
                }
                return 0;*/
            }

            boost::uint64_t end() const
            {
            }

            boost::uint64_t offset(
                SegmentPosition & position) const   // Source开始位置相对于所有数据的位置
            {
                if (position.next_child) {
                    return size_before(position.next_child) - position.next_child->insert_offset_;
                } else {
                    return size_before(position.next_child) - total_size();
                }
            }

            boost::system::error_code seek (
                boost::uint64_t offset,
                SegmentPosition & position, 
                boost::system::error_code & ec) const
            {
                position.next_child = NULL;
                SourceTreeItem * child = this->first_child_;
                while (child) {
                    if (child->insert_offset_ > offset) {
                        position.next_child = child;
                    }
                }
            }

            boost::uint64_t insert_offset() const
            {
                return insert_offset_;
            }

        public:
            virtual boost::uint64_t total_size() const = 0;

        private:
            bool remove_child(
                SourceBase * child,
                SegmentPosition & read_pos,
                SegmentPosition & write_pos,
                bool is_first)
            {
                SourceTreeItem * child_item = (SourceTreeItem *) child;
                SourceTreeItem * source = child_item;
                if (is_first && child_item->parent_) {
                    source = child_item->parent_;
                }
                SourceTreeItem * item = source->first_child_;
                while (item) {
                    if (item == child_item) {
                        if (item->prev_sibling_) {
                            if (item->next_sibling_) {
                                item->prev_sibling_->next_sibling_ = item->next_sibling_;
                                item->next_sibling_->prev_sibling_ = item->prev_sibling_;
                            } else {
                                item->prev_sibling_->next_sibling_ = NULL;
                            }
                        } else {
                            source->first_child_ = item->next_sibling_;
                            if (item->next_sibling_) {
                                item->next_sibling_->prev_sibling_ = NULL;
                            }
                        }
                        if (read_pos.next_child == item) {
                            read_pos.next_child = item->next_sibling_;
                        }
                        if (write_pos.next_child == item) {
                            write_pos.next_child = item->next_sibling_;
                        }
                        return true;
                    }
                    remove_child((SourceBase *) item, read_pos, write_pos, false);
                    item = item->next_sibling_;
                }
                return false;
            }

            boost::uint64_t total_tree_size() const // 自己和所有子节点的size总和
            {
                boost::uint64_t total = total_size();
                SourceTreeItem * item = first_child_;
                while (item) {
                    total += item->total_tree_size();
                    item = item->next_sibling_;
                }
                return total;
            }

            boost::uint64_t size_before(
                SourceTreeItem * child) const // 
            {
                boost::uint64_t total = 0;
                if (parent_) {
                    total += parent_->size_before(const_cast<SourceTreeItem *>(this));
                }
                if (child) {
                    total += child->insert_offset_;
                    SourceTreeItem * item = child->prev_sibling_;
                    while (item) {
                        total += item->total_tree_size();
                        item = item->prev_sibling_;
                    }
                } else {
                    total += total_tree_size();
                }
                return total;
            }

        private:
            boost::uint64_t insert_offset_;    // 插入在父节点的位置，相对于父节点的数据起始位置
            SourceTreeItem * parent_;         // 父节点
            SourceTreeItem * first_child_;    // 第一个子节点
            SourceTreeItem * prev_sibling_;    // 前一个兄弟节点
            SourceTreeItem * next_sibling_;   // 下一个兄弟节点
        };

        class SourceBase
            : public SourceTreeItem
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
            SourceBase(
                boost::asio::io_service & io_svc, 
                boost::uint16_t port)
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

            virtual void on_seg_close(
                size_t segment) {}

        public:
            virtual Segment & operator [](
                size_t segment) = 0;

            virtual Segment const & operator [](
                size_t segment) const = 0;

            virtual size_t total_segments() const = 0;

        public:
            virtual boost::uint64_t total_size() const
            {
                boost::uint64_t total = 0;
                for (size_t seg = 0; seg < total_segments(); seg++) {
                    total += (*this)[seg].file_length;
                }
                return total;
            }

        public:
            boost::system::error_code offset_of_segment(
                boost::uint64_t & offset,
                SegmentPosition & position, 
                boost::system::error_code & ec) const
            {
                SourceBase * source = const_cast<SourceBase *>(this);
                boost::uint64_t total_offset = position.seg_off;
                assert((position.segment < source->total_segments()&& offset <= (*source)[position.segment].file_length)
                    || (position.segment == source->total_segments()&& offset == 0));
                if ((position.segment >= source->total_segments()|| offset > (*source)[position.segment].file_length)
                    && (position.segment != source->total_segments()|| offset != 0))
                    return ec = framework::system::logic_error::out_of_range;
                for (size_t i = 0; i < position.segment; ++i) {
                    if ((*source)[i].total_state < Segment::is_valid)
                        return ec = framework::system::logic_error::out_of_range;
                    total_offset += (*source)[i].file_length;
                }
                position.seg_beg = total_offset - position.seg_off;
                position.seg_end = 
                    (position.segment < source->total_segments()&& 
                    (*source)[position.segment].total_state >= Segment::is_valid) ? 
                    position.seg_beg + (*source)[position.segment].file_length : boost::uint64_t(-1);
                offset = total_offset;
                return ec = boost::system::error_code();
            }

            boost::system::error_code offset_to_segment(
                boost::uint64_t offset,
                SegmentPosition & position,
                boost::system::error_code & ec) const
            {
                SourceBase * source = const_cast<SourceBase *>(this);
                boost::uint64_t seg_offset = offset;
                size_t segment = 0;
                if (position.next_child && offset >= position.next_child->insert_offset()) {
                    return ec = framework::system::logic_error::out_of_range;
                }
                for (segment = 0; segment < source->total_segments()
                    && (*source)[segment].total_state >= Segment::is_valid 
                    && (*source)[segment].file_length <= offset; ++segment)
                    offset -= (*source)[segment].file_length;
                // 增加offset==0，使得position.offset为所有分段总长时，也认为是有效的
                assert(segment < source->total_segments()|| offset == 0);
                if (segment < source->total_segments()|| offset == 0) {
                    position.segment = segment;
                    position.seg_beg = offset - seg_offset + source->offset(position);
                    position.seg_end = 
                        (segment < source->total_segments()&& 
                        (*source)[position.segment].total_state >= Segment::is_valid) ? 
                        position.seg_beg + (*source)[position.segment].file_length : boost::uint64_t(-1);
                    return ec = boost::system::error_code();
                } else {
                    return ec = framework::system::logic_error::out_of_range;
                }
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENTS_BASE_H_

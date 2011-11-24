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

        class SourceTreeItem;

        struct SegmentPosition
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
            SourceTreeItem * next_child;// 处理流程中的下一个子节点
        };

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

        protected:
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
                SourceTreeItem * child,
                SegmentPosition & read_pos,
                SegmentPosition & write_pos)
            {
                return remove_child(child, read_pos, write_pos, true);
            }

            SourceTreeItem * next_source(
                SegmentPosition & position) const // 返回下一个Source
            {
                if (position.next_child) {
                    SourceTreeItem * next = position.next_child;
                    position.next_child = next->first_child_;
                    //position.seg_off = next->offset(position);
                    return next;
                } else {
                    position.next_child = next_sibling_;
                    return parent_;
                }
            }

            bool remove_self(
                SegmentPosition & read_pos,
                SegmentPosition & write_pos)
            {
                return parent_->remove_child(this, read_pos, write_pos);
            }

            bool remove_child(
                SourceTreeItem * child,
                SegmentPosition & read_pos,
                SegmentPosition & write_pos,
                bool is_first)
            {
                SourceTreeItem * source = child;
                if (is_first && child->parent_) {
                    source = child->parent_;
                }
                SourceTreeItem * item = source->first_child_;
                while (item) {
                    if (item == child) {
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
                    remove_child(item, read_pos, write_pos, false);
                    item = item->next_sibling_;
                }
                return false;
            }

            boost::system::error_code seek (
                size_t segment, 
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

        public:
            boost::uint64_t insert_offset_;    // 插入在父节点的位置，相对于父节点的数据起始位置
            boost::uint64_t insert_time_;    // 插入在父节点的位置，相对于父节点的数据起始位置，微妙
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
            virtual SourceBase * next_source(
                SegmentPosition & position)
            {
                SourceBase * next =  (SourceBase *)SourceTreeItem::next_source(position);
                return next;
            }

            virtual void * next_segment(
                SegmentPosition & position)
            {
            }

            virtual boost::system::error_code seek (
                size_t segment, 
                boost::uint64_t offset, 
                SegmentPosition & position, 
                boost::system::error_code & ec)
            {
                SourceTreeItem::seek(segment, offset, position, ec);
            }

        private:
            boost::uint64_t offset(
                SegmentPosition & position)   // Source开始位置相对于所有数据的位置
            {
                if (position.next_child) {
                    return size_before(position.next_child) - position.next_child->insert_offset_;
                } else {
                    return size_before(position.next_child) - total_size();
                }
            }

            boost::uint64_t total_tree_size() // 自己和所有子节点的size总和
            {
                boost::uint64_t total = total_size();
                SourceBase * item = (SourceBase *)first_child_;
                while (item) {
                    total += item->total_tree_size();
                    item = (SourceBase *)item->next_sibling_;
                }
                return total;
            }

            boost::uint64_t size_before(
                SourceTreeItem * child) // 
            {
                boost::uint64_t total = 0;
                if (parent_) {
                    total += ((SourceBase *)parent_)->size_before(this);
                }
                if (child) {
                    total += child->insert_offset_;
                    SourceBase * item = (SourceBase *)child->prev_sibling_;
                    while (item) {
                        total += item->total_tree_size();
                        item = (SourceBase *)item->prev_sibling_;
                    }
                } else {
                    total += total_tree_size();
                }
                return total;
            }

        public:
            //virtual Segment & operator [](
            //    size_t segment) = 0;

            //virtual Segment const & operator [](
            //    size_t segment) const = 0;

            virtual size_t total_segments() const = 0;

            virtual void file_length(
                size_t segment,
                boost::uint64_t file_length) {}

        public:
            virtual boost::uint64_t total_size() const = 0;

        private:
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
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENTS_BASE_H_

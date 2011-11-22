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

        class SourceBase;

        class SegmentTreeItem
        {
        public:
            SegmentTreeItem()
                : insert_offset_(0)
                , parent_(NULL)
                , first_child_(NULL)
                , pre_sibling_(NULL)
                , next_sibling_(NULL)
                , read_next_child_(NULL)
            {
            }

        public:
            bool insert_child(
                boost::uint64_t position, // 相对于自身（被插入节点）的数据起始位置
                boost::uint64_t write_pos,
                SourceBase * child)
            {
                SegmentTreeItem * insert = (SegmentTreeItem *) child;
                insert->parent_ = this;
                if (first_child_) {
                    SegmentTreeItem * item = first_child_;
                    while (item) {
                        if (item->insert_offset_ > position) {
                            if (item->pre_sibling_) {
                                item->pre_sibling_->next_sibling_ = insert;
                                insert->pre_sibling_ = item->pre_sibling_;
                                insert->next_sibling_ = item;
                                insert->insert_offset_ = position;
                                item->pre_sibling_ = insert;
                            } else {
                                first_child_ = insert;
                                insert->next_sibling_ = item;
                                insert->insert_offset_ = position;
                                item->pre_sibling_ = insert;
                            }
                            break;
                        } else {
                            item = item->next_sibling_;
                            if (!item) {
                                item->next_sibling_ = insert;
                                insert->pre_sibling_ = item;
                                insert->insert_offset_ = position;
                                break;
                            }
                        }
                    }
                } else {
                    first_child_ = insert;
                    first_child_->insert_offset_ = position;
                }
                if (position > write_pos) {
                    if (insert->pre_sibling_) {
                        if (write_pos > insert->pre_sibling_->insert_offset_) {
                            read_next_child_ = insert;
                        }
                    } else {
                        read_next_child_ = insert;
                    }
                }
                return true;
            }

            bool remove_child(
                SourceBase * child)
            {
                return remove_child(child, true);
            }

            SourceBase * next_source() // 返回下一个Source
            {
                if (read_next_child_) {
                    read_next_child_ = read_next_child_->next_sibling_;
                    return (SourceBase *)read_next_child_;
                } else {
                    if (parent_)
                        return (SourceBase *)parent_;
                    return NULL;
                }
            }

            bool remove_self()
            {
                return parent_->remove_child((SourceBase *) this);
            }

            boost::uint64_t begin() const
            {
                if (read_next_child_) {
                    return read_next_child_->insert_offset_;
                }
                return 0;
            }

            boost::uint64_t end() const
            {
            }

            boost::uint64_t offset() const   // 相对于所有数据的位置
            {
                if (read_next_child_) {
                    if (read_next_child_->pre_sibling_) {
                        return size_before(read_next_child_->pre_sibling_)
                            - read_next_child_->pre_sibling_->insert_offset_;
                    }
                    return 0;
                }
                SegmentTreeItem * last_child = this->first_child_;
                while (last_child) {
                    last_child = last_child->next_sibling_;
                }
                if (last_child) {
                    return size_before(last_child) - last_child->insert_offset_;
                }
                return 0;
            }

            boost::uint64_t size_before(
                SegmentTreeItem * child) const
            {
                boost::uint64_t total = 0;
                before_size(child, total);
                return total;
            }

            boost::system::error_code seek (
                boost::uint64_t offset,
                boost::system::error_code & ec)
            {
                SegmentTreeItem * child = this->first_child_;
                read_next_child_ = NULL;
                while (child) {
                    if (child->insert_offset_ > offset) {
                        read_next_child_ = child;
                    }
                }
            }

        public:
            virtual boost::uint64_t total_size() const = 0;

        private:
            bool remove_child(
                SourceBase * child,
                bool is_first)
            {
                SegmentTreeItem * child_item = (SegmentTreeItem *) child;
                SegmentTreeItem * source = child_item;
                if (is_first && child_item->parent_) {
                    source = child_item->parent_;
                }
                SegmentTreeItem * item = source->first_child_;
                while (item) {
                    if (item == child_item) {
                        if (item->pre_sibling_) {
                            if (item->next_sibling_) {
                                item->pre_sibling_->next_sibling_ = item->next_sibling_;
                                item->next_sibling_->pre_sibling_ = item->pre_sibling_;
                            } else {
                                item->pre_sibling_->next_sibling_ = NULL;
                            }
                        } else {
                            source->first_child_ = item->next_sibling_;
                            if (item->next_sibling_) {
                                item->next_sibling_->pre_sibling_ = NULL;
                            }
                        }
                        if (source->read_next_child_ == item) {
                            source->read_next_child_ = item->next_sibling_;
                        }
                        return true;
                    }
                    remove_child((SourceBase *) item, false);
                    item = item->next_sibling_;
                }
                return false;
            }

            void total_tree_size(
                SegmentTreeItem * src,
                boost::uint64_t & total) const // 自己和所有子节点的size总和
            {
                total += src->total_size();
                SegmentTreeItem * item = src->first_child_;
                while (item) {
                    total_tree_size(item, total);
                    item = item->next_sibling_;
                }
            }

            void before_size(
                SegmentTreeItem * src,
                boost::uint64_t & total) const
            {
                total += src->insert_offset_;
                SegmentTreeItem * item = src;
                while (item) {
                    total_tree_size(item, total);
                    item = item->pre_sibling_;
                }
                SegmentTreeItem * parent = src->parent_;
                while (parent) {
                    total += parent->insert_offset_;
                    item = parent->pre_sibling_;
                    while (item) {
                        total_tree_size(item, total);
                        item = item->pre_sibling_;
                    }
                    parent = parent->parent_;
                }
            }

        private:
            boost::uint64_t insert_offset_;    // 插入在父节点的位置，相对于父节点的数据起始位置
            SegmentTreeItem * parent_;         // 父节点
            SegmentTreeItem * first_child_;    // 第一个子节点
            SegmentTreeItem * pre_sibling_;    // 前一个兄弟节点
            SegmentTreeItem * next_sibling_;   // 下一个兄弟节点
            SegmentTreeItem * read_next_child_;// 下载流程中的下一个子节点
        };

        class SourceBase
            : public SegmentTreeItem
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
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENTS_BASE_H_
// SourceBase.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/base/BytesStream.h"

#include <framework/system/LogicError.h>

namespace ppbox
{
    namespace demux
    {

        SourceBase::SourceBase(
            boost::asio::io_service & io_svc)
            : insert_segment_(0)
            , insert_size_(0)
            , insert_delta_(0)
            , insert_time_(0)
            , insert_input_time_(0)
        {
        }

        SourceBase::~SourceBase()
        {
        }

        void SourceBase::next_segment(
            SegmentPositionEx & segment)
        {
            SourceBase * next_child = (SourceBase *)segment.next_child;
            if (segment.segment == 0 && !segment.source) {
                segment.source = this;
                segment.shard_beg = segment.size_beg = segment.size_beg;
                segment.shard_end = segment.size_end = segment.size_beg + segment_size(segment.segment);
                segment.time_beg = segment.time_beg;
                segment.time_end = segment.time_beg + segment_time(segment.segment);
            } else if (next_child 
                && (!next_child->skip_)
                && next_child->insert_segment_ == segment.segment) { // 父切子
                    segment.size_end = segment.shard_end;
                    next_source(segment);
                    segment.segment = (size_t)-1;
                    ((SourceBase *)segment.source)->next_segment(segment);
            } else if (++segment.segment == segment_count()) { // 子切父
                next_source(segment);
                if (segment.source) {
                    segment.segment = insert_segment_ - 1;
                    segment.size_end -= insert_size_ - insert_delta_; // 伪造上一段的结尾
                    ((SourceBase *)segment.source)->next_segment(segment);
                    segment.shard_beg = segment.size_beg + insert_size_ - insert_delta_;
                    segment.time_end -= insert_time_;
                }
            } else {
                segment.size_beg = segment.size_end;
                segment.size_end = segment.size_beg + segment_size(segment.segment);
                segment.shard_beg = segment.size_beg;
                if (next_child 
                    && (!next_child->skip_)
                    && next_child->insert_segment_ == segment.segment) {
                        segment.shard_end = segment.size_beg + insert_size_;
                } else {
                    segment.shard_end = segment.size_end;
                }
                segment.time_beg = segment.time_end;
                segment.time_end = segment.time_beg + segment_time(segment.segment);
            }
            if (segment.size_end != (boost::uint64_t)-1) {
                segment.total_state = SegmentPositionEx::is_valid;
            } else {
                segment.total_state = SegmentPositionEx::not_exist;
            }
        }

        boost::system::error_code SourceBase::time_seek (
            boost::uint64_t time, 
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            boost::uint64_t time2 = time;
            boost::uint64_t skip_size = 0;
            SourceBase * next_item = (SourceBase *)first_child_;
            SourceBase * prev_item = NULL;
            while (next_item) {
                if (next_item->skip_) {
                    next_item = (SourceBase *)next_item->next_sibling_;
                    continue;
                }
                boost::uint64_t insert_time = source_time_before(next_item->insert_segment_) + next_item->insert_time_;
                if (time2 < insert_time) {
                    break;
                } else if (time2 < insert_time + next_item->tree_time()) {
                    boost::uint64_t insert_size = source_size_before(next_item->insert_segment_) 
                        + skip_size + next_item->insert_size_;
                    next_item->time_seek(time - insert_time, position, ec);
                    position.size_beg += insert_size;
                    position.size_end += insert_size;
                    position.shard_beg += insert_size;
                    position.shard_end += insert_size;
                    position.time_beg += insert_time;
                    position.time_end += insert_time;
                    return ec;
                } else {
                    time2 -= next_item->tree_time();
                    skip_size += next_item->tree_size() + next_item->insert_delta_;
                }
                prev_item = next_item;
                next_item = (SourceBase *)next_item->next_sibling_;
            }
            SourceTreeItem::seek(position, prev_item, next_item);
            position.segment = 1;
            while (position.segment <= segment_count() && time2 >= source_time_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment > segment_count()) {
                ec = framework::system::logic_error::out_of_range; 
            } else {
                --position.segment;
                position.shard_beg = position.size_beg = skip_size + source_size_before(position.segment);
                position.shard_end = position.size_end = position.size_beg + segment_size(position.segment);
                position.time_beg = time + source_time_before(position.segment) - time2;
                if (prev_item && prev_item->insert_segment_ == position.segment) {
                    position.shard_beg = position.size_beg + prev_item->insert_size_ - prev_item->insert_delta_;
                    position.time_beg = position.time_beg + prev_item->insert_time_;
                }
                position.time_end = position.time_beg + segment_time(position.segment);
                if (next_item && next_item->insert_segment_ == position.segment) {
                    position.shard_end = position.size_beg + next_item->insert_size_;
                    position.time_end = position.time_beg + next_item->insert_time_;
                }
                if (position.size_end != (boost::uint64_t)-1) {
                    position.total_state = SegmentPositionEx::is_valid;
                } else {
                    position.total_state = SegmentPositionEx::not_exist;
                }
            }

            return ec;
        }

        boost::system::error_code SourceBase::size_seek (
            boost::uint64_t size, 
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            boost::uint64_t size2 = size;
            boost::uint64_t skip_time = 0;
            SourceBase * next_item = (SourceBase *)first_child_;
            SourceBase * prev_item = NULL;
            while (next_item) {
                if (next_item->skip_) {
                    next_item = (SourceBase *)next_item->next_sibling_;
                    continue;
                }
                boost::uint64_t insert_size = source_size_before(next_item->insert_segment_) + next_item->insert_size_;
                if (size2 < insert_size) {
                    break;
                } else if (size2 < insert_size + next_item->tree_size()) {
                    boost::uint64_t insert_time = source_time_before(next_item->insert_segment_) 
                        + skip_time + next_item->insert_time_;
                    next_item->size_seek(size - insert_size, position, ec);
                    position.size_beg += insert_size;
                    position.size_end += insert_size;
                    position.shard_beg += insert_size;
                    position.shard_end += insert_size;
                    position.time_beg += insert_time;
                    position.time_end += insert_time;
                    return ec;
                } else {
                    size2 -= next_item->tree_size() + next_item->insert_delta_;
                    skip_time += next_item->tree_time();
                }
                prev_item = next_item;
                next_item = (SourceBase *)next_item->next_sibling_;
            }
            assert(next_item == NULL || size2 < next_item->insert_size_ + position.size_end);
            SourceTreeItem::seek(position, prev_item, next_item);
            position.segment = 1;
            while (position.segment < segment_count() && size2 >= source_size_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment > segment_count()) {
                ec = framework::system::logic_error::out_of_range; 
            }
            --position.segment;
            position.shard_beg = position.size_beg = size + source_size_before(position.segment) - size2;
            position.shard_end = position.size_end = position.size_beg + segment_size(position.segment);
            position.time_beg = skip_time + source_time_before(position.segment);
            if (prev_item && prev_item->insert_segment_ == position.segment) {
                position.shard_beg = position.size_beg + prev_item->insert_size_ - prev_item->insert_delta_;
                position.time_beg = position.time_beg + prev_item->insert_time_;
            }
            position.time_end = position.time_beg + segment_time(position.segment);
            if (next_item && next_item->insert_segment_ == position.segment) {
                position.shard_end = position.size_beg + next_item->insert_size_;
                position.time_end = position.time_beg + next_item->insert_time_;
            }
            if (position.size_end != (boost::uint64_t)-1) {
                position.total_state = SegmentPositionEx::is_valid;
            } else {
                position.total_state = SegmentPositionEx::not_exist;
            }
            return ec;
        }

        boost::system::error_code SourceBase::time_insert(
            boost::uint32_t time, 
            SourceBase * source, 
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            time_seek(time, position, ec);
            if (!ec) {
                source->skip_ = true;// 假插入
                source->insert_segment_ = position.segment;// 指向父分段
                source->insert_input_time_ = source->insert_time_ = time - position.time_beg;   // 插入分段上的时间位置（相对）
                position.source->insert_child(source, position.next_child);                     // 插入子节点
            }
            return ec;
        }

        void SourceBase::update_insert(
            SegmentPositionEx const & position, 
            boost::uint32_t time, 
            boost::uint64_t offset, 
            boost::uint64_t delta)
        {
            skip_ = false; // 真插入
            insert_time_ = time - position.time_beg; // 插入的时间点（相对本分段开始位置位置）
            insert_size_ = offset;
            insert_delta_ = delta;
        }

        boost::uint64_t SourceBase::next_end(
            SegmentPositionEx & segment)
        {
            assert(segment.source == this);
            next_skip_source(segment);
            if (segment.source) {
                return segment.source->total_size_before((SourceBase *)segment.next_child);
            } else {
                return (boost::uint64_t)-1;
            }
        }

        boost::uint64_t SourceBase::source_size()
        {
            boost::uint64_t total = 0;
            for (int i = 0; i < segment_count(); i++) {
                total += segment_size(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_size_before(
            size_t segment)
        {
            boost::uint64_t total = 0;
            if (segment > segment_count()) {
                segment = segment_count();
            }
            for (int i = 0; i< segment; i++) {
                total += segment_size(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_time()
        {
            boost::uint64_t total = 0;
            for (int i = 0; i< segment_count(); i++) {
                total += segment_time(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_time_before(
            size_t segment)
        {
            boost::uint64_t total = 0;
            if (segment > segment_count()) {
                segment = segment_count();
            }
            for (int i = 0; i < segment; i++) {
                total += segment_time(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_size()
        {
            boost::uint64_t total = source_size();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_size();
                    total += item->insert_delta_;
                }
                item = (SourceBase *)item->next_sibling_;
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_time()
        {
            boost::uint64_t total = source_time();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_time();
                    item = (SourceBase *)item->next_sibling_;
                }
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_size_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total += source_size_before(child->insert_segment_) + child->insert_size_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_size();
                        total += item->insert_delta_; // 需要加入前半部分的数据才能正常播放
                    }
                    item = (SourceBase *)item->prev_sibling_;
                }
            } else {
                total += tree_size();
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_time_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total = source_time_before(child->insert_segment_) + child->insert_time_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_time();
                        item = (SourceBase *)item->prev_sibling_;
                    }
                }
            } else {
                total = tree_time();
            }
            return total;
        }

        boost::uint64_t SourceBase::total_size_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((SourceBase *)parent_)->tree_size_before(this);
            }
            total += tree_size_before(child);
            return total;
        }

        boost::uint64_t SourceBase::total_time_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((SourceBase *)parent_)->total_time_before(this);
            }
            total += tree_time_before(child);
            return total;
        }

    } // namespace demux
} // namespace ppbox

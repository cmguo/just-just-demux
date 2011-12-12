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
        {
        }

        SourceBase::~SourceBase()
        {
        }

        void SourceBase::next_segment(
            SegmentPosition & segment)
        {
            SourceBase * next_child = (SourceBase *)segment.next_child;
            if (next_child 
                && next_child->insert_segment_ == segment.segment
                && segment.size_beg + next_child->insert_size_ == segment.size_end) {
                    next_source(segment);
                    segment.segment = (size_t)-1;
                    ((SourceBase *)segment.source)->next_segment(segment);
            } else if (segment.segment == 0 && !segment.source) {
                segment.source = this;
                segment.size_beg = segment.size_beg;
                segment.size_end = segment.size_beg + segment_size(segment.segment);
                segment.time_beg = segment.time_beg;
                segment.time_end = segment.time_beg + segment_time(segment.segment);
            } else if (++segment.segment == segment_count()) {
                next_source(segment);
                if (segment.source) {
                    segment.segment = insert_segment_ - 1;
                    segment.size_end -= insert_size_;
                    ((SourceBase *)segment.source)->next_segment(segment);
                }
            } else {
                segment.size_beg = segment.size_end;
                segment.size_end = segment.size_beg + segment_size(segment.segment);
                segment.time_beg = segment.time_end;
                segment.time_end = segment.time_beg + segment_time(segment.segment);
            }
            segment.total_state = SegmentPosition::is_valid;
        }

        boost::system::error_code SourceBase::time_seek (
            boost::uint64_t time, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
            boost::uint64_t time2 = time;
            boost::uint64_t skip_size = 0;
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                boost::uint64_t insert_time = source_time_before(item->insert_segment_) + item->insert_time_;
                if (time2 < insert_time) {
                    break;
                } else if (time2 < insert_time + item->tree_time()) {
                    return item->time_seek(time - insert_time_, position, ec);
                } else {
                    time2 -= item->tree_time();
                    skip_size += item->tree_size();
                }
                item = (SourceBase *)item->next_sibling_;
            }
            SourceTreeItem::seek(position, item);
            position.segment = 1;
            while (position.segment < segment_count() && time2 >= source_time_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment > segment_count()) {
                ec = framework::system::logic_error::out_of_range; 
            }
            --position.segment;
            position.size_beg = skip_size + source_size_before(position.segment);
            position.size_end = position.size_beg + segment_size(position.segment);
            position.time_beg = time + source_time_before(position.segment) - time2;
            position.time_end = position.time_beg + segment_time(position.segment);
            if (item && item->insert_segment_ == position.segment) {
                position.size_end = position.size_beg + item->insert_size_;
                position.time_end = position.time_beg + item->insert_time_;
            }
            position.total_state = SegmentPosition::is_valid;
            return ec;
        }

        boost::system::error_code SourceBase::size_seek (
            boost::uint64_t size, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
            boost::uint64_t size2 = size;
            boost::uint64_t skip_time = 0;
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                boost::uint64_t insert_size = source_size_before(item->insert_segment_) + item->insert_size_;
                if (size2 < insert_size) {
                    break;
                } else if (size2 < insert_size + item->tree_size()) {
                    return item->time_seek(size2 - item->insert_size_, position, ec);
                } else {
                    size2 -= item->tree_size();
                    skip_time += item->tree_time();
                }
                item = (SourceBase *)item->next_sibling_;
            }
            assert(item == NULL || size2 < item->insert_size_);
            SourceTreeItem::seek(position, item);
            position.segment = 1;
            while (position.segment < segment_count() && size2 >= source_size_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment > segment_count()) {
                ec = framework::system::logic_error::out_of_range; 
            }
            --position.segment;
            position.size_beg = size + source_size_before(position.segment) - size2;
            position.size_end = position.size_beg + segment_size(position.segment);
            position.time_beg = skip_time + source_time_before(position.segment);
            position.time_end = position.time_beg + segment_time(position.segment);
            if (item && item->insert_segment_ == position.segment) {
                position.size_end = position.size_beg + item->insert_size_;
                position.time_end = position.size_beg + item->insert_time_;
            }
            position.total_state = SegmentPosition::is_valid;
            return ec;
        }

        boost::system::error_code SourceBase::time_insert(
            boost::uint32_t time, 
            SourceBase * source, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
            time_seek(time, position, ec);
            if (!ec) {
                source->insert_segment_ = position.segment;
                source->insert_time_ = time - position.time_beg;
                position.source->insert_child(source, position.next_child);
            }
            return ec;
        }

        void SourceBase::update_insert(
            boost::uint64_t offset, 
            boost::uint64_t delta)
        {
            insert_size_ = offset;
            insert_delta_ = delta;
        }

        boost::uint64_t SourceBase::next_end(
            SourceBase * source)
        {
            SourceBase * child = (SourceBase *)source->first_child_;
            while (!child) {
                if (source->next_sibling_
                    && ((SourceBase *)source->next_sibling_)->insert_segment_ == source->insert_segment_) {
                        child = source->next_sibling()->first_child();
                        continue;
                } else if (source->next_sibling_) {
                    child = source->next_sibling()->first_child();
                }
                break;
            }
            if (child) {
                SourceBase * parent = (SourceBase *)child->parent_;
                boost::uint64_t end = parent->segment_head_size(child->insert_segment_) 
                    + parent->source_size_before(child->insert_segment_);
                if (parent->parent_) {
                    SourceBase * root = (SourceBase *)parent->parent_;
                    end += root->segment_head_size(parent->insert_segment_) 
                        + root->source_size_before(parent->insert_segment_);
                }
                return end;
            }
            return boost::uint64_t(-1);
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
                if (item->insert_size_) {
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
                total += item->tree_time();
                item = (SourceBase *)item->next_sibling_;
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
                    if (item->insert_size_) {
                        total += item->tree_size();
                        total += item->insert_delta_;
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
                    total += item->tree_time();
                    item = (SourceBase *)item->prev_sibling_;
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

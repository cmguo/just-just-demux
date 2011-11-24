// SourceBase.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceBase.h"

namespace ppbox
{
    namespace demux
    {

        SourceBase::SourceBase(
            boost::asio::io_service & io_svc)
        {
        }

        SourceBase::~SourceBase()
        {
        }

        void SourceBase::next_segment(
            SegmentPosition & position)
        {
            if (position.next_child 
                && position.next_child->insert_segment_ == position.segment
                && position.seg_beg + position.next_child->insert_offset_ == position.seg_end)
                    next_source(position);
                    position.segment = (size_t)-1;
                    ((SourceBase *)position.source)->next_segment(position);
            } else if (++position.segment == segment_count()) {
                next_source(position);
                position.segment = insert_segment_ - 1;
                position.seg_end -= insert_offset_;
                ((SourceBase *)position.source)->next_segment(position);
            } else {
                position.seg_beg = position.seg_end;
                position.seg_end = position.seg_beg + segment_size(position.segment);
            }
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
                if (time2 < item->insert_time_) {
                    break;
                } else if (time2 < item->insert_time_ + item->tree_time()) {
                    return item->time_seek(time - insert_time, position, ec);
                } else {
                    time2 -= item->tree_time();
                    skip_size += item->tree_size();
                }
                item = (SourceBase *)item->next_sibling_;
            }
            assert(item == NULL || time2 < item->insert_time_);
            SourceTreeItem::seek(position, item);
            position.segment = 1;
            while (position.segment < segment_count() && time2 >= source_time_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment >= segment_count()) {
                ec = logic_error::out_of_range; 
            }
            --position.segment;
            position.seg_beg = skip_size + source_time_before(position.segment);
            position.seg_end = position.seg_beg + segment_size(position.segment);
            position.time_beg = time + source__time_before(position.segment) - time2;
            position.time_end = position.time_beg + segment_time(position.segment);
            if (item && item->insert_segment = position.segment) {
                position.seg_end = position.seg_beg + item->insert_offset_;
                position.time_end = position.seg_beg + item->insert_time_;
            }
            return ec;
        }

        boost::system::error_code SourceBase::offset_seek (
            boost::uint64_t size, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
            boost::uint64_t size2 = size;
            boost::uint64_t skip_time = 0;
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                if (size2 < item->insert_offset_) {
                    break;
                } else if (size2 < item->insert_offset_ + item->tree_size()) {
                    return item->time_seek(time - item->insert_offset_, position, ec);
                } else {
                    size2 -= item->tree_size();
                    skip_time += item->tree_time();
                }
                item = (SourceBase *)item->next_sibling_;
            }
            assert(item == NULL || size2 < item->insert_offset_);
            SourceTreeItem::seek(position, item);
            position.segment = 1;
            while (position.segment < segment_count() && size2 >= source_size_before(position.segment)) {
                ++position.segment;
            }
            if (position.segment >= segment_count()) {
                ec = logic_error::out_of_range; 
            }
            --position.segment;
            position.seg_beg = size + source_size_before(position.segment) - size2;
            position.seg_end = position.seg_beg + segment_size(position.segment);
            position.time_beg = skip_time + source_time_before(position.segment);
            position.time_end = position.time_beg + segment_time(position.segment);
            if (item && item->insert_segment = position.segment) {
                position.seg_end = position.seg_beg + item->insert_offset_;
                position.time_end = position.seg_beg + item->insert_time_;
            }
            return ec;
        }

        boost::uint64_t SourceBase::tree_size()
        {
            boost::uint64_t total = source_size();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                total += item->tree_size();
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
            SourceTreeItem * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total += source_size_before(child.segment) + child->insert_offset_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    total += item->total_tree_size();
                    total += item->insert_delta_;
                    item = (SourceBase *)item->prev_sibling_;
                }
            } else {
                total += tree_size();
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_time_before(
            SourceTreeItem * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total = source_time_before(child.segment) + child->insert_time_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    total += item->total_tree_time();
                    item = (SourceBase *)item->prev_sibling_;
                }
            } else {
                total = tree_time();
            }
            return total;
        }

        boost::uint64_t SourceBase::total_size_before(
            SourceTreeItem * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((SourceBase *)parent_)->tree_size_before(this);
            }
            total += tree_size_before(child);
            return total;
        }

        boost::uint64_t SourceBase::total_time_before(
            SourceTreeItem * child)
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

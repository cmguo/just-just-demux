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
        }

        boost::system::error_code SourceBase::offset_seek (
            boost::uint64_t offset, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
        }

        boost::uint64_t SourceBase::offset(
            SegmentPosition const & position)
        {
            if (position.next_child) {
                return size_before(position.next_child) - position.next_child->insert_offset_;
            } else {
                return size_before(position.next_child) - total_size();
            }
        }

        boost::uint64_t SourceBase::total_tree_size()
        {
            boost::uint64_t total = total_size();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                total += item->total_tree_size();
                item = (SourceBase *)item->next_sibling_;
            }
            return total;
        }

        boost::uint64_t SourceBase::size_before(
            SourceTreeItem * child)
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
                    total += item->insert_delta_;
                    item = (SourceBase *)item->prev_sibling_;
                }
            } else {
                total += total_tree_size();
            }
            return total;
        }

    } // namespace demux
} // namespace ppbox

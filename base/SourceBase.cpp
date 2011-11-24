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

        void * SourceBase::next_segment(
            SegmentPosition & position)
        {
        }

        boost::system::error_code SourceBase::seek (
            size_t segment, 
            boost::uint64_t offset, 
            SegmentPosition & position, 
            boost::system::error_code & ec)
        {
            SourceTreeItem::seek(segment, offset, position, ec);
        }

        boost::uint64_t SourceBase::offset(
            SegmentPosition & position)   // Source开始位置相对于所有数据的位置
        {
            if (position.next_child) {
                return size_before(position.next_child) - position.next_child->insert_offset_;
            } else {
                return size_before(position.next_child) - total_size();
            }
        }

        boost::uint64_t SourceBase::total_tree_size() // 自己和所有子节点的size总和
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

    } // namespace demux
} // namespace ppbox

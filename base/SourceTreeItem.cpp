// SourceTreeItem.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceTreeItem.h"

namespace ppbox
{
    namespace demux
    {

        bool SourceTreeItem::insert_child(
            SourceTreeItem * child, 
            SourceTreeItem * where)
        {
            child->parent_ = this;
            child->next_sibling_ = where;
            if (where) {
                where->prev_sibling_ = child;
                child->prev_sibling_ = where->prev_sibling_;
                where->prev_sibling_->next_sibling_ = child;
            } else {
                child->prev_sibling_ = NULL;
                first_child_ = child;
            }
        }

        void SourceTreeItem::remove_child(
            SourceTreeItem * child)
        {
            assert(child->parent_ == this);
            if (child->prev_sibling_) {
                child->prev_sibling_->next_sibling_ = child->next_sibling_;
            } else {
                first_child_ = child->next_sibling_;
            }
            if (child->next_sibling_) {
                child->next_sibling_->prev_sibling_ = child->prev_sibling_;
            }
            child->prev_sibling_ = child->next_sibling_ = NULL;
        }

        void SourceTreeItem::next_source(
            SegmentPosition & position) const
        {
            if (position.next_child) {
                position.source = position.next_child;
                position.next_child = position.source->first_child_;
            } else {
                position.source = parent_;
                position.next_child = next_sibling_;
            }
        }

        void SourceTreeItem::remove_self(
            SegmentPosition & read_pos,
            SegmentPosition & write_pos)
        {
            return parent_->remove_child(this);
        }

        void SourceTreeItem::seek (
            SegmentPosition & position, 
            SourceTreeItem * where) const
        {
            position.source = this;
            position.next_child = where;
        }

    } // namespace demux
} // namespace ppbox

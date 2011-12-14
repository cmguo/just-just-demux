// SourceTreeItem.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceTreeItem.h"
#include "ppbox/demux/base/SourceBase.h"

namespace ppbox
{
    namespace demux
    {

        void SourceTreeItem::insert_child(
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
            SourceTreePosition & position) const
        {
            assert(position.source == this);
            position.source = NULL;
            while (position.next_child) {
                if (!position.next_child->skip_) {
                    position.source = (SourceBase *)position.next_child;
                    position.next_child = position.next_child->first_child_;
                    break;
                }
                position.next_child = position.next_child->next_sibling_;
            }
            if (!position.source) {
                position.source = (SourceBase *)parent_;
                position.next_child = next_sibling_;
            }
        }

        void SourceTreeItem::next_skip_source(
            SourceTreePosition & position) const
        {
            if (position.next_child) {
                position.source = (SourceBase *)position.next_child;
                position.next_child = position.next_child->first_child_;
                if (position.source->skip_) {
                    return;
                } else {
                    return position.source->next_skip_source(position);
                }
            } else {
                position.source = (SourceBase *)parent_;
                position.next_child = next_sibling_;
                if (position.source) {
                    assert(!position.source->skip_);
                    return position.source->next_skip_source(position);
                } else {
                    return;
                }
            }
        }

        void SourceTreeItem::seek (
            SourceTreePosition & position, 
            SourceTreeItem * where) const
        {
            position.source = static_cast<SourceBase *>(const_cast<SourceTreeItem *>((this)));
            position.next_child = where;
        }

    } // namespace demux
} // namespace ppbox

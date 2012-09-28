// SourceTreeItem.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceTreeItem.h"

#include <ppbox/data/SourceBase.h>

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
                if (where->prev_sibling_) {
                    where->prev_sibling_->next_sibling_ = child;
                } else {
                    first_child_ = child;
                }
            } else {
                child->prev_sibling_ = last_child_;
                if (last_child_) {
                    last_child_->next_sibling_ = child;
                } else {
                    first_child_ = child;
                }
                last_child_ = child;
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
            SourceTreePosition & position)
        {
            if (!position.current) { // 回滚到开始
                assert(position.prev_child == this);
                position.current = this;
                position.prev_child = NULL;
                position.next_child = first_child_;
                return;
            }
            assert(position.current == this);
            position.current = NULL;
            while (position.next_child) { // 父切子
                position.current = position.next_child;
                position.prev_child = NULL;
                position.next_child = position.next_child->first_child_;
            }
            if (!position.current) { // 子切父
                position.current = parent_;
                position.prev_child = this;
                position.next_child = next_sibling_;
            }
        }

        void SourceTreeItem::seek(
            SourceTreePosition & position, 
            SourceTreeItem * where_prev, 
            SourceTreeItem * where)
        {
            position.current = this;
            position.prev_child = where_prev;
            position.next_child = where;
        }

    } // namespace demux
} // namespace ppbox

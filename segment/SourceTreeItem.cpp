// SourceTreeItem.cpp

#include "just/demux/Common.h"
#include "just/demux/segment/SourceTreeItem.h"

namespace just
{
    namespace demux
    {

        void SourceTreeItem::insert_child(
            SourceTreeItem * child, 
            SourceTreeItem * where)
        {
            SourceTreeItem * my_part = child->next_;
            child->parent_ = this;
            my_part->parent_ = parent_;
            my_part->owner_ = owner_;
            if (where) {
                insert_list(child, where->prev_, my_part);
                insert_list(my_part, child, where);
            } else {
            }
        }

        void SourceTreeItem::remove_child(
            SourceTreeItem * child)
        {
        }

        void SourceTreeItem::insert_list(
            SourceTreeItem * item, 
            SourceTreeItem * prev, 
            SourceTreeItem * next)
        {
            item->prev_ = prev;
            item->next_ = next;
            if (prev)
                prev->next_ = item;
            if (next)
                next->prev_ = item;
        }

        void SourceTreeItem::remove_list(
            SourceTreeItem * item)
        {
            if (item->prev_)
                item->prev_->next_ = item->next_;
            if (item->next_)
                item->next_->prev_ = item->prev_;
            item->prev_ = item->next_ = NULL;
        }


    } // namespace demux
} // namespace just

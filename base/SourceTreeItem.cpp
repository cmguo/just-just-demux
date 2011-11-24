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
            if (first_child_) {
                SourceTreeItem * item = first_child_;
                while (item) {
                    if (item->insert_offset_ > position) {
                        if (item->prev_sibling_) {
                            item->prev_sibling_->next_sibling_ = child;
                            child->prev_sibling_ = item->prev_sibling_;
                            child->next_sibling_ = item;
                            child->insert_offset_ = position;
                            item->prev_sibling_ = child;
                        } else {
                            first_child_ = child;
                            child->next_sibling_ = item;
                            child->insert_offset_ = position;
                            item->prev_sibling_ = child;
                        }
                        break;
                    } else {
                        item = item->next_sibling_;
                        if (!item) {
                            item->next_sibling_ = child;
                            child->prev_sibling_ = item;
                            child->insert_offset_ = position;
                            break;
                        }
                    }
                }
            } else {
                first_child_ = child;
                first_child_->insert_offset_ = position;
            }

            if (position > write_offset) {
                if (write_pos.next_child) {
                    if (position < write_pos.next_child->insert_offset_) {
                        write_pos.next_child = child;
                    }
                } else {
                    write_pos.next_child = child;
                }
            }
            if (position > read_offset) {
                if (read_pos.next_child) {
                    if (position < read_pos.next_child->insert_offset_) {
                        read_pos.next_child = child;
                    }
                } else {
                    read_pos.next_child = child;
                }
            }
            return true;
        }

        bool SourceTreeItem::remove_child(
            SourceTreeItem * child,
            SegmentPosition & read_pos,
            SegmentPosition & write_pos)
        {
            return remove_child(child, read_pos, write_pos, true);
        }

        SourceTreeItem * SourceTreeItem::next_source(
            SegmentPosition & position) const // 返回下一个Source
        {
            if (position.next_child) {
                SourceTreeItem * next = position.next_child;
                position.next_child = next->first_child_;
                //position.seg_off = next->offset(position);
                return next;
            } else {
                position.next_child = next_sibling_;
                return parent_;
            }
        }

        bool SourceTreeItem::remove_self(
            SegmentPosition & read_pos,
            SegmentPosition & write_pos)
        {
            return parent_->remove_child(this, read_pos, write_pos);
        }

        bool SourceTreeItem::remove_child(
            SourceTreeItem * child,
            SegmentPosition & read_pos,
            SegmentPosition & write_pos,
            bool is_first)
        {
            SourceTreeItem * source = child;
            if (is_first && child->parent_) {
                source = child->parent_;
            }
            SourceTreeItem * item = source->first_child_;
            while (item) {
                if (item == child) {
                    if (item->prev_sibling_) {
                        if (item->next_sibling_) {
                            item->prev_sibling_->next_sibling_ = item->next_sibling_;
                            item->next_sibling_->prev_sibling_ = item->prev_sibling_;
                        } else {
                            item->prev_sibling_->next_sibling_ = NULL;
                        }
                    } else {
                        source->first_child_ = item->next_sibling_;
                        if (item->next_sibling_) {
                            item->next_sibling_->prev_sibling_ = NULL;
                        }
                    }
                    if (read_pos.next_child == item) {
                        read_pos.next_child = item->next_sibling_;
                    }
                    if (write_pos.next_child == item) {
                        write_pos.next_child = item->next_sibling_;
                    }
                    return true;
                }
                remove_child(item, read_pos, write_pos, false);
                item = item->next_sibling_;
            }
            return false;
        }

        boost::system::error_code SourceTreeItem::seek (
            size_t segment, 
            boost::uint64_t offset, 
            SegmentPosition & position, 
            boost::system::error_code & ec) const
        {
            position.next_child = NULL;
            SourceTreeItem * child = this->first_child_;
            while (child) {
                if (child->insert_offset_ > offset) {
                    position.next_child = child;
                }
            }
        }

    } // namespace demux
} // namespace ppbox

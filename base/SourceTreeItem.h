// SegmentsBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_
#define _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

namespace ppbox
{
    namespace demux
    {

        class SourceTreeItem;
        class DemuxStrategy;

        struct SourceTreePosition
        {
            SourceTreePosition()
                : current(NULL)
                , prev_child(NULL)
                , next_child(NULL)
            {
            }

            SourceTreeItem * current;   // 处理流程中的当前
            SourceTreeItem * prev_child;// 处理流程中的上一个子节点
            SourceTreeItem * next_child;// 处理流程中的下一个子节点

            friend bool operator==(
                SourceTreePosition const & l, 
                SourceTreePosition const & r)
            {
                return l.current == r.current
                    && l.next_child == r.next_child
                    && l.prev_child == r.prev_child;
            }

            friend bool operator<(
                SourceTreePosition const & l, 
                SourceTreePosition const & r)
            {
                return false;
            }

            friend bool operator!=(
                SourceTreePosition const & l, 
                SourceTreePosition const & r)
            {
                return l.current != r.current
                    || l.next_child != r.next_child
                    || l.prev_child != r.prev_child;
            }

        };

        class SourceTreeItem
        {
        public:
            SourceTreeItem(
                DemuxStrategy * owner)
                : owner_(owner)
                , parent_(NULL)
                , first_child_(NULL)
                , last_child_(NULL)
                , prev_sibling_(NULL)
                , next_sibling_(NULL)
            {
            }

        public:
            void insert_child(
                SourceTreeItem * child, 
                SourceTreeItem * where);

            void remove_child(
                SourceTreeItem * child);

            // 返回下一个Source
            void next_source(
                SourceTreePosition & position);

            void seek(
                SourceTreePosition & position, 
                SourceTreeItem * where_prev, 
                SourceTreeItem * where);

            void remove_self()
            {
                return parent_->remove_child(this);
            }

        public:
            DemuxStrategy * owner() const
            {
                return owner_;
            }

        public:
            DemuxStrategy * owner_;         // 对应的Strategy对象，包含该Item的对象
            SourceTreeItem * parent_;       // 父节点
            SourceTreeItem * first_child_;  // 第一个子节点
            SourceTreeItem * last_child_;   // 最后一个子节点
            SourceTreeItem * prev_sibling_; // 前一个兄弟节点
            SourceTreeItem * next_sibling_; // 下一个兄弟节点
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

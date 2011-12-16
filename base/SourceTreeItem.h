// SegmentsBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_
#define _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

namespace ppbox
{
    namespace demux
    {

        class SourceTreeItem;
        class SourceBase;

        struct SourceTreePosition
        {
            SourceBase * source;// 处理流程中的下一个子节点
            SourceTreeItem * prev_child;// 处理流程中的上一个子节点
            SourceTreeItem * next_child;// 处理流程中的下一个子节点

            friend bool operator == (
                SourceTreePosition const & l, 
                SourceTreePosition const & r)
            {
                return l.source == r.source
                    && l.next_child == r.next_child;
            }

            friend bool operator != (
                SourceTreePosition const & l, 
                SourceTreePosition const & r)
            {
                return l.source != r.source
                    || l.next_child != r.next_child;
            }

        };

        class SourceTreeItem
        {
        public:
            SourceTreeItem()
                : parent_(NULL)
                , first_child_(NULL)
                , prev_sibling_(NULL)
                , next_sibling_(NULL)
                , skip_(false)
            {
            }

        protected:
            void insert_child(
                SourceTreeItem * child, 
                SourceTreeItem * where);

            void remove_child(
                SourceTreeItem * child);

            // 返回下一个Source
            void next_source(
                SourceTreePosition & position) const;

            void next_skip_source(
                SourceTreePosition & position) const;

            void seek(
                SourceTreePosition & position, 
                SourceTreeItem * where_prev, 
                SourceTreeItem * where) const;

            void remove_self()
            {
                return parent_->remove_child(this);
            }

        protected:
            SourceTreeItem * parent_;         // 父节点
            SourceTreeItem * first_child_;    // 第一个子节点
            SourceTreeItem * prev_sibling_;    // 前一个兄弟节点
            SourceTreeItem * next_sibling_;   // 下一个兄弟节点
            bool skip_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

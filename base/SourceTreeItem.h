// SegmentsBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_
#define _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

namespace ppbox
{
    namespace demux
    {

        class SourceTreeItem;

        struct SourceTreePosition
        {
            SourceTreeItem * next_child;// 处理流程中的下一个子节点
        };

        class SourceTreeItem
        {
        public:
            SourceTreeItem()
                : insert_offset_(0)
                , parent_(NULL)
                , first_child_(NULL)
                , prev_sibling_(NULL)
                , next_sibling_(NULL)
            {
            }

        protected:
            void insert_child(
                SourceTreeItem * child, 
                SourceTreeItem * where);

            bool remove_child(
                SourceTreeItem * child)

            // 返回下一个Source
            SourceTreeItem * next_source(
                SourceTreePosition & position) const;

            bool remove_self()
            {
                return parent_->remove_child(this);
            }

        protected:
            SourceTreeItem * parent_;         // 父节点
            SourceTreeItem * first_child_;    // 第一个子节点
            SourceTreeItem * prev_sibling_;    // 前一个兄弟节点
            SourceTreeItem * next_sibling_;   // 下一个兄弟节点
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

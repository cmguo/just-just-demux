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
            SourceBase * source;// ���������е���һ���ӽڵ�
            SourceTreeItem * next_child;// ���������е���һ���ӽڵ�
        };

        class SourceTreeItem
        {
        public:
            SourceTreeItem()
                : parent_(NULL)
                , first_child_(NULL)
                , prev_sibling_(NULL)
                , next_sibling_(NULL)
            {
            }

        protected:
            void insert_child(
                SourceTreeItem * child, 
                SourceTreeItem * where);

            void remove_child(
                SourceTreeItem * child);

            // ������һ��Source
            void next_source(
                SourceTreePosition & position) const;

            void seek(
                SourceTreePosition & position, 
                SourceTreeItem * where) const;

            void remove_self()
            {
                return parent_->remove_child(this);
            }

        protected:
            SourceTreeItem * parent_;         // ���ڵ�
            SourceTreeItem * first_child_;    // ��һ���ӽڵ�
            SourceTreeItem * prev_sibling_;    // ǰһ���ֵܽڵ�
            SourceTreeItem * next_sibling_;   // ��һ���ֵܽڵ�
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_
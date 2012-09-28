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

            SourceTreeItem * current;   // ���������еĵ�ǰ
            SourceTreeItem * prev_child;// ���������е���һ���ӽڵ�
            SourceTreeItem * next_child;// ���������е���һ���ӽڵ�

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

            // ������һ��Source
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
            DemuxStrategy * owner_;         // ��Ӧ��Strategy���󣬰�����Item�Ķ���
            SourceTreeItem * parent_;       // ���ڵ�
            SourceTreeItem * first_child_;  // ��һ���ӽڵ�
            SourceTreeItem * last_child_;   // ���һ���ӽڵ�
            SourceTreeItem * prev_sibling_; // ǰһ���ֵܽڵ�
            SourceTreeItem * next_sibling_; // ��һ���ֵܽڵ�
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

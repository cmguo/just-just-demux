// SegmentsBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_
#define _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

namespace ppbox
{
    namespace demux
    {

        class DemuxStrategy;

        class SourceTreeItem
        {
        public:
            SourceTreeItem(
                DemuxStrategy * owner)
                : owner_(owner)
                , parent_(NULL)
                , prev_(NULL)
                , next_(NULL)
            {
            }

        public:
            void insert_child(
                SourceTreeItem * child, 
                SourceTreeItem * where);

            void remove_child(
                SourceTreeItem * child);

            SourceTreeItem * parent()
            {
                return parent_;
            }

            // ������һ��Source
            SourceTreeItem * next()
            {
                return next_;
            }

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
            friend bool operator<(
                SourceTreeItem const & l, 
                SourceTreeItem const & r);

        private:
            void insert_list(
                SourceTreeItem * item, 
                SourceTreeItem * prev, 
                SourceTreeItem * next);

            void remove_list(
                SourceTreeItem * item);

        private:
            DemuxStrategy * owner_;         // ��Ӧ��Strategy���󣬰�����Item�Ķ���
            SourceTreeItem * parent_;       // ���ڵ�
            //SourceTreeItem * first_child_;  // ��һ���ӽڵ�
            //SourceTreeItem * last_child_;   // ���һ���ӽڵ�
            //SourceTreeItem * prev_sibling_; // ǰһ���ֵܽڵ�
            //SourceTreeItem * next_sibling_; // ��һ���ֵܽڵ�
            SourceTreeItem * prev_;         // ǰһ���ڵ�
            SourceTreeItem * next_;         // ��һ���ڵ�
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_TREE_ITEM_H_

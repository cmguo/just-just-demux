// SourceTreeItem.h

#ifndef _JUST_DEMUX_SEGMENT_SOURCE_TREE_ITEM_H_
#define _JUST_DEMUX_SEGMENT_SOURCE_TREE_ITEM_H_

namespace just
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

            // 返回下一个Source
            SourceTreeItem * next()
            {
                return next_;
            }

            void remove_self()
            {
                return parent_->remove_child(this);
            }

            bool is_inserted() const
            {
                // next_->parent_ 不一定 == this
                return next_ != NULL && next_->parent_->owner_ == owner_;
            }

            DemuxStrategy * next_owner() const
            {
                return next_->owner();
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
            DemuxStrategy * owner_;         // 对应的Strategy对象，包含该Item的对象
            SourceTreeItem * parent_;       // 父节点
            //SourceTreeItem * first_child_;  // 第一个子节点
            //SourceTreeItem * last_child_;   // 最后一个子节点
            //SourceTreeItem * prev_sibling_; // 前一个兄弟节点
            //SourceTreeItem * next_sibling_; // 下一个兄弟节点
            SourceTreeItem * prev_;         // 前一个节点
            SourceTreeItem * next_;         // 下一个节点
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_SEGMENT_SOURCE_TREE_ITEM_H_

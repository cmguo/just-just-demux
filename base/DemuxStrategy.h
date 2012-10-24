// DemuxStrategy.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

#include "ppbox/demux/base/SourceTreeItem.h"

#include <ppbox/data/SegmentStrategy.h>

namespace ppbox
{
    namespace demux
    {

        struct SegmentPosition
            : ppbox::data::SegmentPosition
        {
            SourceTreeItem * item() const
            {
                return (SourceTreeItem *)item_context;
            }

            DemuxStrategy * owner() const
            {
                return item()->owner();
            }

            SourceTreeItem * next_item() const
            {
                return item()->next();
            }

            DemuxStrategy * next_owner() const
            {
                return next_item()->owner();
            }

            bool is_inserted() const
            {
                // next_item()->parent_ 不一定 == this
                return next_item() != NULL && next_item()->parent()->owner() == owner();
            }
        };

        struct DemuxerInfo;

        // 功能需求:
        //  1、管理多个源；
        //  2、支持顺序遍历分段；
        //  3、提供头部大小、分段大小、时长等信息（可选）；
        //  4、支持输入绝对位置（time || size）返回分段信息；
        //  5、打开（range）、关闭、读取、取消功能（若不提供分段大小，则打开成功后获取分段大小）；
        //  6、提供插入和删除源
        class DemuxStrategy
            : public ppbox::data::SegmentStrategy
        {
        public:

            static DemuxStrategy * create(
                boost::asio::io_service & io_svc, 
                framework::string::Url const & playlink);

            static void destory(
                DemuxStrategy * sourcebase);

        public:
            DemuxStrategy(
                ppbox::data::MediaBase & media);

            ~DemuxStrategy();

        public:
            boost::system::error_code insert(
                SegmentPosition const & pos, 
                DemuxStrategy & child, 
                boost::system::error_code & ec);

        public:
            virtual bool next_segment(
                ppbox::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                return next_segment((SegmentPosition &)pos, ec);
            }

            //virtual bool byte_seek(
            //    boost::uint64_t offset, 
            //    ppbox::data::SegmentPosition & pos, 
            //    boost::system::error_code & ec)
            //{
            //    SegmentPosition base;
            //    return ec;
            //}

            virtual bool time_seek(
                boost::uint64_t offset, 
                ppbox::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                SegmentPosition base;
                return time_seek(offset, base, (SegmentPosition &)pos, ec);
            }

            virtual boost::uint64_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool next_segment(
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                SegmentPosition & base,
                SegmentPosition & pos, 
                boost::system::error_code & ec);

        public:
            boost::system::error_code time_insert(
                boost::uint32_t time, 
                DemuxStrategy * source, 
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            void update_insert(
                SegmentPosition const & pos, 
                boost::uint32_t time, 
                boost::uint64_t offset, 
                boost::uint64_t delta);

            boost::uint64_t next_end(
                SegmentPosition & segment);

        private:
            SourceTreeItem tree_item_;
            SourceTreeItem insert_item_;    // 代表父节点被切割的后面一个部分
            DemuxerInfo * insert_demuxer_;  // 父节点的demuxer
            size_t insert_segment_;         // 插入在父节点的分段
            boost::uint64_t insert_size_;   // 插入在分段上的偏移位置，相对于分段起始位置（无法）
            boost::uint64_t insert_delta_;  // 需要重复下载的数据量
            boost::uint64_t insert_time_;   // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

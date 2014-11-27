// DemuxStrategy.h

#ifndef _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_
#define _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_

#include "just/demux/segment/SourceTreeItem.h"

#include <just/data/segment/SegmentStrategy.h>

namespace just
{
    namespace demux
    {

        struct DemuxerInfo;

        // 功能需求:
        //  1、管理多个源；
        //  2、支持顺序遍历分段；
        //  3、提供头部大小、分段大小、时长等信息（可选）；
        //  4、支持输入绝对位置（time || size）返回分段信息；
        //  5、打开（range）、关闭、读取、取消功能（若不提供分段大小，则打开成功后获取分段大小）；
        //  6、提供插入和删除源
        class DemuxStrategy
            : public just::data::SegmentStrategy
        {
        public:

            static DemuxStrategy * create(
                boost::asio::io_service & io_svc, 
                framework::string::Url const & playlink);

            static void destory(
                DemuxStrategy * sourcebase);

        public:
            DemuxStrategy(
                just::data::SegmentMedia & media);

            ~DemuxStrategy();
/*
        public:
            boost::system::error_code insert(
                just::data::SegmentPosition const & pos, 
                DemuxStrategy & child, 
                boost::system::error_code & ec);
*/
        public:
            virtual bool next_segment(
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec);

            //virtual bool byte_seek(
            //    boost::uint64_t offset, 
            //    just::data::SegmentPosition & pos, 
            //    boost::system::error_code & ec)
            //{
            //    SegmentPosition base;
            //    return ec;
            //}

            virtual bool time_seek(
                boost::uint64_t offset, 
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                just::data::SegmentPosition base;
                return time_seek(offset, base, pos, ec);
            }

            virtual boost::uint64_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                just::data::SegmentPosition & base,
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec);

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
} // namespace just

#endif // _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_

// DemuxStrategy.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

#include "ppbox/demux/base/SourceTreeItem.h"

#include <ppbox/data/MediaBase.h>
#include <ppbox/data/strategy/Strategy.h>

#include <framework/timer/TimeCounter.h>

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;

        struct SegmentPosition
            : SourceTreePosition
            , ppbox::data::SegmentInfoEx
        {
            boost::uint64_t big_time;
            boost::uint64_t time_beg;
            boost::uint64_t time_end;

            SegmentPosition()
                : big_time(0)
                , time_beg(0)
                , time_end(0)
            {
            }

            boost::uint64_t big_time_beg() const
            {
                return big_time + time_beg;
            }

            boost::uint64_t big_time_end() const
            {
                return (time_end == boost::uint64_t(-1)) 
                    ? boost::uint64_t(-1) : (big_time + time_end);
            }

            bool is_same_segment( 
                SegmentPosition const & r) const
            {
                return ((SourceTreePosition const &)(*this) == (SourceTreePosition const &)r 
                    && this->index == r.index);
            }

            friend bool operator<(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return ((SourceTreePosition const &)l < (SourceTreePosition const &)r 
                    || ((SourceTreePosition const &)l == (SourceTreePosition const &)r 
                    && (ppbox::data::SegmentInfoEx const &)l < (ppbox::data::SegmentInfoEx const &)r));
            }

            friend bool operator==(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return ((SourceTreePosition const &)l == (SourceTreePosition const &)r 
                    && (ppbox::data::SegmentInfoEx const &)l == (ppbox::data::SegmentInfoEx const &)r);
            }

            friend bool operator!=(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return !(l == r);
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
            : ppbox::data::Strategy
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

            ppbox::data::MediaBase & media()
            {
                return media_;
            }

        public:
            virtual bool next_segment(
                ppbox::data::SegmentInfoEx & info)
            {
                return false;
            }

            virtual boost::system::error_code byte_seek(
                size_t offset,
                ppbox::data::SegmentInfoEx & info, 
                boost::system::error_code & ec)
            {
                return ec;
            }

            virtual boost::system::error_code time_seek(
                boost::uint32_t time_ms, 
                ppbox::data::SegmentInfoEx & info, 
                boost::system::error_code & ec)
            {
                return ec;
            }

            virtual std::size_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                SegmentPosition & base,
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            virtual bool next_segment(
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
            ppbox::data::MediaBase & media_;
            framework::timer::TimeCounter counter_; 
            DemuxerInfo * insert_demuxer_;  // 父节点的demuxer
            size_t insert_segment_;         // 插入在父节点的分段
            boost::uint64_t insert_size_;   // 插入在分段上的偏移位置，相对于分段起始位置（无法）
            boost::uint64_t insert_delta_;  // 需要重复下载的数据量
            boost::uint64_t insert_time_;   // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

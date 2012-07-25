// Content.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_BASE_H_
#define _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/SourceTreeItem.h"
#include "ppbox/demux/base/SourcePrefix.h"

#include <ppbox/common/SourceBase.h>
#include <ppbox/common/SegmentBase.h>

#include <util/buffers/Buffers.h>

#include <boost/asio/buffer.hpp>

#include <map>

namespace ppbox
{
    namespace demux
    {
        struct SegmentPosition
            : SourceTreePosition
        {
            SegmentPosition()
                : segment(size_t(-1))
            {
            }

            friend bool operator == (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition const &)l == (SourceTreePosition const &)r
                    && l.segment == r.segment;
            }

            friend bool operator != (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition const &)l != (SourceTreePosition const &)r
                    || l.segment != r.segment;
            }

            size_t segment; // 分段
        };

        struct SegmentPositionEx
            : SegmentPosition
        {
            SegmentPositionEx()
                : total_state(not_init)
                , time_state(not_init)
                , size_beg(0)
                , size_end(boost::uint64_t(-1))
                , time_beg(0)
                , time_end(boost::uint64_t(-1))
                , shard_beg(0)
                , shard_end(boost::uint64_t(-1))
            {
            }

            enum StateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 

                unknown_time,
                unknown_size,
            };

            StateEnum total_state;
            StateEnum time_state;  
            boost::uint64_t size_beg; // 全局的偏移
            boost::uint64_t size_end; // 全局的偏移 
            boost::uint64_t time_beg; // 全局的偏移
            boost::uint64_t time_end; // 全局的偏移
            boost::uint64_t shard_beg; //碎片的起始
            boost::uint64_t shard_end; //碎片的结束
        };

        struct DemuxerInfo
        {
            SegmentPosition segment;
            DemuxerBase * demuxer;
            bool is_read_stream;
            boost::uint32_t ref;
        };

        struct Event
        {
            enum EventType
            {
                // 下载
                EVENT_SEG_DL_OPEN,      // 分段下载打开成功
                EVENT_SEG_DL_BEGIN,     // 开始下载分段
                EVENT_SEG_DL_END,       // 结束下载分段

                // 解封装
                EVENT_SEG_DEMUXER_OPEN, // 分段解封装打开成功
                EVENT_SEG_DEMUXER_PLAY, // 开始播放分段
                EVENT_SEG_DEMUXER_STOP, // 结束播放分段
            };

            Event(
                EventType type,
                SegmentPositionEx seg,
                boost::system::error_code ec)
                : evt_type(type)
                , seg_info(seg)
                , ec(ec)
            {
            }

            EventType evt_type;
            SegmentPositionEx seg_info;
            boost::system::error_code ec;
        };

        class BufferList;
        class BufferDemuxer;

        // 功能需求:
        //  1、管理多个源；
        //  2、支持顺序遍历分段；
        //  3、提供头部大小、分段大小、时长等信息（可选）；
        //  4、支持输入绝对位置（time || size）返回分段信息；
        //  5、打开（range）、关闭、读取、取消功能（若不提供分段大小，则打开成功后获取分段大小）；
        //  6、提供插入和删除源
        class Content
            : public SourceTreeItem
        {
        public:

            static Content * create(
                boost::asio::io_service & io_svc, std::string const & playlink);

            static void destory(
                Content * sourcebase);

        public:
            Content(
                boost::asio::io_service & io_svc
                ,ppbox::common::SegmentBase* segment
                ,ppbox::common::SourceBase* source);

            ~Content();

        public:
            ppbox::common::SegmentBase* get_segment();

            ppbox::common::SourceBase* get_source();

            virtual DemuxerType::Enum demuxer_type() const = 0;


        public:
            virtual void on_error(
                boost::system::error_code & ec) {}

            // 处理事件通知
            virtual void on_event(
                Event const & evt);

        public:
            bool has_children(
                SegmentPositionEx const & position);

            virtual boost::system::error_code reset(
                SegmentPositionEx & segment) = 0;

            virtual bool next_segment(
                SegmentPositionEx & position);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // 微妙
                SegmentPositionEx & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code size_seek (
                boost::uint64_t size,  
                SegmentPositionEx const & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

        //private:
        public:
            // 自己和所有子节点的size总和
            virtual boost::uint64_t tree_size();

            virtual boost::uint64_t source_time_before(
                size_t segment);

            // 所有节点在child插入点之前的size总和
            virtual boost::uint64_t total_size_before(
                Content * child);

            // 所有节点在child插入点之前的time总和
            virtual boost::uint64_t total_time_before(
                Content * child);

        public:
            boost::system::error_code time_insert(
                boost::uint32_t time, 
                Content * source, 
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            void update_insert(
                SegmentPositionEx const & position, 
                boost::uint32_t time, 
                boost::uint64_t offset, 
                boost::uint64_t delta);

            boost::uint64_t next_end(
                SegmentPositionEx & segment);

        public:
            void set_buffer_list(
                BufferList * buffer)
            {
                buffer_ = buffer;
            }

        public:
            boost::uint64_t insert_size() const
            {
                return insert_size_;
            }

            size_t insert_segment() const
            {
                return insert_segment_;
            }

            boost::uint64_t insert_time() const
            {
                return insert_time_;
            }

            boost::uint64_t insert_input_time() const
            {
                return insert_input_time_;
            }

            Content * parent()
            {
                return (Content *)parent_;
            }

            Content * next_sibling()
            {
                return (Content *)next_sibling_;
            }

            Content * prev_sibling()
            {
                return (Content *)prev_sibling_;
            }

            Content * first_child()
            {
                return (Content *)first_child_;
            }

            DemuxerInfo & insert_demuxer()
            {
                return insert_demuxer_;
            }

        public:

            // 自己所有分段的size总和
            virtual boost::uint64_t source_size();

            // 指定分段之前所有分段大小总和
            virtual boost::uint64_t source_size_before(
                size_t segment);

            // 自己所有分段的time总和
            virtual boost::uint64_t source_time();

            virtual boost::uint64_t tree_size_before(
                Content * child);

            // 自己和所有子节点的time总和
            virtual boost::uint64_t tree_time();

            virtual boost::uint64_t tree_time_before(
                Content * child);

        protected:
            boost::asio::io_service & ios_service()
            {
                return io_svc_;
            }

            BufferList * buffer()
            {
                return buffer_;
            }

            BufferDemuxer * demuxer()
            {
                return demuxer_;
            }

        protected:
            SegmentPositionEx begin_segment_;

        private:
            BufferList * buffer_;
            ppbox::common::SegmentBase * segment_;
            ppbox::common::SourceBase * source_;
            boost::asio::io_service & io_svc_;
            DemuxerInfo insert_demuxer_;// 父节点的demuxer
            BufferDemuxer * demuxer_;
            size_t insert_segment_; // 插入在父节点的分段
            boost::uint64_t insert_size_; // 插入在分段上的偏移位置，相对于分段起始位置（无法）
            boost::uint64_t insert_delta_; // 需要重复下载的数据量
            boost::uint64_t insert_time_; // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
            boost::uint64_t insert_input_time_;
            std::map<std::string, SourcePrefix::Enum> type_map_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

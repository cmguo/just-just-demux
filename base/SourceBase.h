// SourceBase.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_BASE_H_
#define _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/SourceTreeItem.h"

#include <util/buffers/Buffers.h>

#include <boost/asio/buffer.hpp>

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
                , size_beg(0)
                , size_end((boost::uint64_t)-1)
                , time_beg(0)
                , time_end((boost::uint64_t)-1)
                , shard_beg(0)
                , shard_end((boost::uint64_t)-1)
            {
            }

            enum TotalStateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 

                unknown_time,   // 未知时长
                unknown_size,   // 未知大小
            };

            TotalStateEnum total_state;
            boost::uint64_t size_beg; // 全局的偏移
            boost::uint64_t size_end; // 全局的偏移
            boost::uint64_t time_beg; // 全局的偏移
            boost::uint64_t time_end; // 全局的偏移
            boost::uint64_t shard_beg; //碎片的起始
            boost::uint64_t shard_end; //碎片的结束
        };

        typedef boost::intrusive_ptr<
            BytesStream> StreamPointer;

        typedef boost::intrusive_ptr<
            DemuxerBase> DemuxerPointer;

        struct DemuxerInfo
        {
            StreamPointer stream;
            DemuxerPointer demuxer;
            SegmentPosition segment;
        };

        struct _tEvent
        {
            enum _eEventType
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

            _tEvent(
                _eEventType type, SegmentPositionEx seg, boost::system::error_code ec)
                : evt_type( type )
                , seg_info( seg )
                , ec( ec )
            {
            }

            _eEventType evt_type;
            SegmentPositionEx seg_info;
            boost::system::error_code ec;
        };

        class BufferList;
        class BufferDemuxer;

        //************************************
        // Method:    SourceBase
        // 功能需求:
        //  1、管理多个源；
        //  2、支持顺序遍历分段；
        //  3、提供头部大小、分段大小、时长等信息（可选）；
        //  4、支持输入绝对位置（time || size）返回分段信息；
        //  5、打开（range）、关闭、读取、取消功能（若不提供分段大小，则打开成功后获取分段大小）；
        //  6、提供插入和删除源
        // FullName:  ppbox::demux::SourceBase::SourceBase
        // Access:    public 
        // Returns:   
        // Qualifier:
        // Parameter: boost::asio::io_service & io_svc
        //************************************
        class SourceBase
            : public SourceTreeItem
        {
        public:
            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

            typedef boost::function<void(
                boost::system::error_code const &,
                size_t)
            > read_handle_type;

        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;


            static SourceBase * create(
                boost::asio::io_service & io_svc, std::string const & playlink);

            static void destory(
                SourceBase * sourcebase);

        public:
            SourceBase(
                boost::asio::io_service & io_svc);

            virtual ~SourceBase();

        public: // BufferList调用
            virtual boost::system::error_code segment_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec) = 0;

            virtual void segment_async_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                response_type const & resp) = 0;

            virtual bool segment_is_open(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t total(
                boost::system::error_code & ec) = 0;

            virtual std::size_t segment_read(
                write_buffer_t const & buffers,
                boost::system::error_code & ec) = 0;

            virtual void segment_async_read(
                write_buffer_t const & buffers,
                read_handle_type handler) = 0;

            virtual bool continuable(
                boost::system::error_code const & ec) = 0;

            virtual bool recoverable(
                boost::system::error_code const & ec) = 0;

            virtual boost::system::error_code segment_cancel(
                size_t segment, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code segment_close(
                size_t segment, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec) = 0;

        public: // BufferDemuxer调用
            virtual boost::system::error_code open(){ return boost::system::error_code(); }

            virtual void async_open(
                response_type const & resp){}

            virtual bool is_open()
            {
                return true;
            }

            virtual void update_segment_duration(
                size_t segment,boost::uint32_t time){}

            virtual void update_segment_file_size(
                size_t segment,boost::uint64_t fsize){}

            virtual void update_segment_head_size(
                size_t segment,boost::uint64_t hsize){}

            virtual DemuxerType::Enum demuxer_type() const = 0;

            virtual void set_url(std::string const &url){}

            virtual boost::system::error_code reset(
                size_t& segment)
            {
                return boost::system::error_code();
            }

            virtual boost::uint32_t get_duration()
            {
                return 0;
            }

        public:
            virtual void on_error(
                boost::system::error_code & ec) {}

            virtual void on_seg_beg(
                size_t segment) {}

            virtual void on_seg_end(
                size_t segment) {}

            // 处理事件通知
            virtual void on_event(
                _tEvent const & evt);

        public:
            virtual bool next_segment(
                SegmentPositionEx & position);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // 微妙
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code size_seek (
                boost::uint64_t size,  
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual boost::uint64_t segment_head_size(
                size_t segment)
            {
                return 0;
            }

            // 自己和所有子节点的size总和
            virtual boost::uint64_t tree_size();

            virtual boost::uint64_t source_time_before(
                size_t segment);

            // 所有节点在child插入点之前的size总和
            virtual boost::uint64_t total_size_before(
                SourceBase * child);

            // 所有节点在child插入点之前的time总和
            virtual boost::uint64_t total_time_before(
                SourceBase * child);

        public:
            boost::system::error_code time_insert(
                boost::uint32_t time, 
                SourceBase * source, 
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

            virtual void set_name(
                std::string const & name)
            {
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

            SourceBase * parent()
            {
                return (SourceBase *)parent_;
            }

            SourceBase * next_sibling()
            {
                return (SourceBase *)next_sibling_;
            }

            SourceBase * prev_sibling()
            {
                return (SourceBase *)prev_sibling_;
            }

            SourceBase * first_child()
            {
                return (SourceBase *)first_child_;
            }

            DemuxerInfo & insert_demuxer()
            {
                return insert_demuxer_;
            }

        private:
            //************************************
            // Method:    segment_count 分段个数
            // FullName:  ppbox::demux::SourceBase::segment_count
            // Access:    virtual private 
            // Returns:   size_t
            // Qualifier: const
            //************************************
            virtual size_t segment_count() const = 0;

            //************************************
            // Method:    segment_size 获取指定分段的大小
            // FullName:  ppbox::demux::SourceBase::segment_size
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier: 
            // Parameter: size_t segment
            //************************************
            virtual boost::uint64_t segment_size(
                size_t segment) = 0;

            //************************************
            // Method:    segment_time 获取指定分段的时长
            // FullName:  ppbox::demux::SourceBase::segment_time
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier: 
            // Parameter: size_t segment
            //************************************
            virtual boost::uint64_t segment_time(
                size_t segment) = 0;

            //************************************
            // Method:    source_size 自己所有分段的size总和
            // FullName:  ppbox::demux::SourceBase::source_size
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            //************************************
            virtual boost::uint64_t source_size();

            //************************************
            // Method:    source_size_before 指定分段之前所有分段大小总和
            // FullName:  ppbox::demux::SourceBase::source_size_before
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            // Parameter: size_t segment
            //************************************
            virtual boost::uint64_t source_size_before(
                size_t segment);

            //************************************
            // Method:    source_time 自己所有分段的time总和
            // FullName:  ppbox::demux::SourceBase::source_time
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            //************************************
            virtual boost::uint64_t source_time();

            //************************************
            // Method:    tree_size_before
            // FullName:  ppbox::demux::SourceBase::tree_size_before
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            // Parameter: SourceBase * child
            //************************************
            virtual boost::uint64_t tree_size_before(
                SourceBase * child);

            //************************************
            // Method:    tree_time 自己和所有子节点的time总和
            // FullName:  ppbox::demux::SourceBase::tree_time
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            //************************************
            virtual boost::uint64_t tree_time();

            //************************************
            // Method:    tree_time_before
            // FullName:  ppbox::demux::SourceBase::tree_time_before
            // Access:    virtual private 
            // Returns:   boost::uint64_t
            // Qualifier:
            // Parameter: SourceBase * child
            //************************************
            virtual boost::uint64_t tree_time_before(
                SourceBase * child);

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

        private:
            boost::asio::io_service & io_svc_;
            DemuxerInfo insert_demuxer_;// 父节点的demuxer
            BufferList * buffer_;
            BufferDemuxer * demuxer_;
            size_t insert_segment_; // 插入在父节点的分段
            boost::uint64_t insert_size_; // 插入在分段上的偏移位置，相对于分段起始位置（无法）
            boost::uint64_t insert_delta_; // 需要重复下载的数据量
            boost::uint64_t insert_time_; // 插入在分段上的时间位置，相对于分段起始位置，单位：微妙
            boost::uint64_t insert_input_time_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

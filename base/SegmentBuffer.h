// SegmentBuffer.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/base/DemuxStrategy.h"
#include "ppbox/demux/base/Buffer.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceError.h"

#include <ppbox/data/strategy/Strategy.h>

#include <util/event/Event.h>

#include <framework/system/LogicError.h>

namespace ppbox
{
    namespace data
    {
        class SegmentSource;
    }

    namespace demux
    {

        class BytesStream;

        class SegmentBuffer
            : public Buffer
            , private BufferObserver
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &,
                size_t)
            > prepare_response_type;

            //typedef ppbox::data::SegmentInfoEx segment_t;
            typedef ppbox::demux::SegmentPosition segment_t;

        public:
            SegmentBuffer(
                ppbox::data::SegmentSource * source, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size);

            ~SegmentBuffer();

            // 目前只发生在，seek到一个分段，还没有该分段头部数据时，
            // 此时size为head_size_头部数据大小
            // TO BE FIXED
            boost::system::error_code seek(
                segment_t const & base,
                segment_t const & pos,
                boost::uint64_t end, 
                boost::system::error_code & ec);

            // seek到分段的具体位置offset
            // TO BE FIXED
            boost::system::error_code seek(
                segment_t const & base,
                segment_t const & pos, 
                boost::system::error_code & ec);

            // Parameter: boost::uint32_t amount 需要下载的数据大小
            // Parameter: boost::system::error_code & ec
            size_t prepare(
                size_t amount, 
                boost::system::error_code & ec);

            size_t prepare_at_least(
                size_t amount, 
                boost::system::error_code & ec)
            {
                return prepare(amount > prepare_size_ ? amount : prepare_size_, ec);
            }

            void async_prepare(
                size_t amount, 
                prepare_response_type const & resp);

            void async_prepare_at_least(
                size_t amount, 
                prepare_response_type const & resp)
            {
                async_prepare(amount > prepare_size_ ? amount : prepare_size_, resp);
            }

            boost::system::error_code data(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec);

            boost::system::error_code drop(
                boost::system::error_code & ec);

            /**
                drop_all 
                丢弃当前分段的所有剩余数据，并且更新当前分段信息
             */
            boost::system::error_code drop_all(
                boost::system::error_code & ec);

            void reset(
                segment_t const & base, 
                segment_t const & pos);

        public:
            BufferStatistic stat() const
            {
                return buffer_stat();
            }

            // 获取最后一次错误的错误码
            boost::system::error_code last_error() const
            {
                return last_ec_;
            }

        public:
            // 写分段
            segment_t const & base_segment() const
            {
                return base_;
            }

            // 读BytesStream
            // 读分段
            segment_t const & read_segment() const
            {
                return read_;
            }

            // 写分段
            segment_t const & write_segment() const
            {
                return write_;
            }

            // 读BytesStream
            BytesStream & read_stream() const
            {
                return *read_stream_;
            }

            // 写BytesStream
            BytesStream & write_stream() const
            {
                return *write_stream_;
            }

            // 读BytesStream
            BytesStream & bytes_stream(
                bool read) const
            {
                return read ? *read_stream_ : *write_stream_;
            }

        private:
            friend class BytesStream;

            struct PositionType
            {
                enum Enum
                {
                    set, // 相对分段文件起始位置
                    beg, // 相对有效数据起始位置
                    end, // 相对有效数据结束位置
                };
            };

            boost::system::error_code segment_buffer(
                segment_t const & segment, 
                PositionType::Enum pos_type, 
                boost::uint64_t & pos, 
                boost::uint64_t & off, 
                boost::asio::const_buffer & buffer);

        private:
            virtual void on_event(
                util::event::Event const & event);

        private:
            void update_read(
                segment_t const & seg);

            void update_write(
                segment_t const & seg);

            void handle_async(
                boost::system::error_code const & ecc, 
                size_t bytes_transferred);

            void dump();

        private:
            ppbox::data::SegmentSource * source_;
            boost::uint32_t prepare_size_;  // 下载一次，最大的读取数据大小

            segment_t base_;
            std::deque<segment_t> segments_;
            segment_t read_;
            segment_t write_;

            BytesStream * read_stream_;    // 读Buffer
            BytesStream * write_stream_;   // 写Buffer

            prepare_response_type resp_;

            boost::system::error_code last_ec_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

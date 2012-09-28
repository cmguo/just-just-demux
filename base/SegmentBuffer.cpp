// SegmentBuffer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SegmentBuffer.h"
//#include "ppbox/demux/base/DemuxStrategy.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceError.h"
#include "ppbox/demux/base/BytesStream.h"

#include <ppbox/data/SegmentSource.h>
using namespace ppbox::data;

#include <framework/system/LogicError.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/bind.hpp>

namespace ppbox
{
    namespace demux
    {

        FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("SegmentBuffer", 0)

        SegmentBuffer::SegmentBuffer(
            ppbox::data::SegmentSource * source, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size)
            : Buffer(buffer_size)
            , source_(source)
            , prepare_size_(prepare_size)
        {
            read_stream_ = new BytesStream(
                *this, read_);
            write_stream_ = new BytesStream(
                *this, write_);
        }

        SegmentBuffer::~SegmentBuffer()
        {
            if (read_stream_)
                delete read_stream_;
            if (write_stream_)
                delete write_stream_;
        }

        // 目前只发生在，seek到一个分段，还没有该分段头部数据时，
        // 此时size为head_size_头部数据大小
        // TO BE FIXED
        boost::system::error_code SegmentBuffer::seek(
            segment_t const & base,
            segment_t const & pos,
            boost::uint64_t size, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (base != base_) {
                boost::system::error_code ec1;
                reset(base, pos);
                //source_->seek(pos, size, ec);
                return ec;
            }

            bool write_change = Buffer::seek(pos.big_pos());

            while (segments_.front().big_end() <= data_begin()) {
                segments_.pop_front();
            }
            while (segments_.back().big_beg() >= data_end()) {
                segments_.pop_back();
            }

            update_read(pos);
            read_stream_->seek(pos.small_offset);

            if (write_change) {
                // 调整了写指针，需要从新的位置开始下载
                if (segments_.back().big_end() > out_position()) {
                    //source_->seek(segments_.back(), size, ec);
                    update_write(segments_.back());
                } else {
                    //source_->byte_seek(out_position(), size, ec);
                    segment_t seg;
                    source_->current_segment(seg);
                    update_write(seg); 
                }
                write_stream_->seek(0);
            }

            return ec;
        }

        // seek到分段的具体位置offset
        // TO BE FIXED
        boost::system::error_code SegmentBuffer::seek(
            segment_t const & base,
            segment_t const & pos, 
            boost::system::error_code & ec)
        {
            return seek(base, pos, (boost::uint64_t)-1, ec);
        }

        //************************************
        // Method:    prepare
        // FullName:  ppbox::demux::SegmentBuffer::prepare
        // Access:    public 
        // Returns:   boost::system::error_code
        // Qualifier:
        // Parameter: boost::uint32_t amount 需要下载的数据大小
        // Parameter: boost::system::error_code & ec
        //************************************
        size_t SegmentBuffer::prepare(
            size_t amount, 
            boost::system::error_code & ec)
        {
            return boost::asio::read(
                *source_, Buffer::prepare(amount), boost::asio::transfer_all(), ec);
        }

        void SegmentBuffer::async_prepare(
            size_t amount, 
            prepare_response_type const & resp)
        {
            resp_ = resp;
            boost::asio::async_read(
                *source_, Buffer::prepare(amount), boost::asio::transfer_all(), 
                boost::bind(&SegmentBuffer::handle_async, this, _1, _2));
        }

        void SegmentBuffer::handle_async(
            boost::system::error_code const & ec, 
            size_t bytes_transferred)
        {
            last_ec_ = ec;
            commit(bytes_transferred);
            prepare_response_type resp;
            resp.swap(resp_);
            resp(ec, 0);
        }

        boost::system::error_code SegmentBuffer::data(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            std::deque<boost::asio::const_buffer> & data, 
            boost::system::error_code & ec)
        {
            offset += read_.big_beg();
            assert(offset >= in_position() && offset + size <= read_.big_end());
            if (offset < in_position()) {
                ec = framework::system::logic_error::out_of_range;
            } else if (offset + size > read_.big_end()) {
                ec = boost::asio::error::eof;
            } else {
                if (offset + size > out_position()) {// 是否超出当前写指针
                    prepare_at_least((boost::uint32_t)(offset + size - out_position()), ec);
                }
                if (offset + size <= out_position()) {
                    read_buffer_t bufs = Buffer::read_buffer(offset, offset + size);
                    data.insert(data.end(), bufs.begin(), bufs.end());
                    ec.clear();
                }
            }
            return ec;
        }

        boost::system::error_code SegmentBuffer::drop(
            boost::system::error_code & ec)
        {
            read_.small_offset = read_stream_->position();
            if (consume((size_t)(read_.big_pos() - in_position()))) {
                read_stream_->update();
                if (read_ == write_)
                    write_stream_->update();
                ec.clear();
            } else {
                ec = framework::system::logic_error::out_of_range;
            }
            return ec;
        }

        /**
        drop_all 
        丢弃当前分段的所有剩余数据，并且更新当前分段信息
        */
        // TO BE FIXED
        boost::system::error_code SegmentBuffer::drop_all(
            boost::system::error_code & ec)
        {
            // TODO
            //if (read_.segment == write_.segment) {
            // source_->drop_all();
            //}
            if (consume((size_t)(read_.big_end() - in_position()))) {
                // TODO
                segments_.pop_front();
                read_ = segments_.front();
                // 读缓冲DropAll
                read_stream_->drop_all();
            } else {
                ec = framework::system::logic_error::out_of_range;
            }
            return ec;
        }

        void SegmentBuffer::reset(
            segment_t const & base, 
            segment_t const & pos)
        {
            base_ = base;
            Buffer::reset(pos.big_pos());
            segments_.clear();
            segments_.push_back(pos);

            read_ = write_ = pos;

            read_stream_->seek(pos.small_offset);
            write_stream_->seek(pos.small_offset);
        }
/*
        void SegmentBuffer::update_base(
            segment_t const & seg, 
            boost::uint32_t offset)
        {
            if (seg == base_)
                return;

            base_ = seg;
            
            read_.size_beg      -= offset;
            read_.shard_beg     -= offset;
            read_.size_beg      -= offset;
            read_.size_end      -= offset;

            write_.size_beg     -= offset;
            write_.shard_beg    -= offset;
            write_.size_end     -= offset;
            write_.shard_end    -= offset;
        }
*/
        boost::system::error_code SegmentBuffer::segment_buffer(
            segment_t const & segment, 
            PositionType::Enum pos_type, 
            boost::uint64_t & pos, 
            boost::uint64_t & off, 
            boost::asio::const_buffer & buffer)
        {
            boost::uint64_t beg = segment.big_beg();
            boost::uint64_t end = segment.big_end();
            if (beg < in_position()) {
                beg = in_position();
            }
            if (end > out_position()) {
                end = out_position();
            }
            boost::system::error_code ec;
            if (pos_type == PositionType::beg) {
                pos += beg;
                if (pos > end) {
                    pos = end;
                    ec = framework::system::logic_error::out_of_range;
                }
            } else if (pos_type == PositionType::end) {
                pos = end - pos;
                if (pos < beg) {
                    pos = beg;
                    ec = framework::system::logic_error::out_of_range;
                }
            } else {
                pos += segment.big_offset;
                if (pos < beg) {
                   pos = beg;
                   ec = framework::system::logic_error::out_of_range;
                } else if (pos > end) {
                    if (pos > segment.big_end()) {
                        pos = segment.big_end();
                        ec = framework::system::logic_error::out_of_range;
                    }
                    if (pos > out_position()) {
                        boost::system::error_code ec1 = last_ec_;
                        ec1 || prepare((size_t)(pos - out_position()), ec1);
                        if (pos > out_position()) {
                            pos = out_position();
                            if (!ec) ec = ec1;
                        }
                        end = segment.big_end();
                        if (end > out_position())
                            end = out_position();
                    }
                    assert(pos <= end);
                }
            }
            char const * ptr = read_buffer(beg, pos, end); // read_buffer里面会调整beg或者end
            off = pos - beg;
            pos = beg - segment.big_offset;
            buffer = boost::asio::const_buffer(ptr, (size_t)(end - beg));
            return ec;
        }

        void SegmentBuffer::on_event(
            util::event::Event const & event)
        {
            //update_write();
        }

        void SegmentBuffer::update_read(
            segment_t const & seg)
        {
            if (!segments_.empty() && segments_.front() == seg) {
                segments_.front() == seg;
            } else {
                segments_.push_front(seg);
            }
            read_ == seg;
            if (write_ == seg) {
                write_ == seg;
            }
        }

        void SegmentBuffer::update_write(
            segment_t const & seg)
        {
            if (!segments_.empty() && segments_.back() == seg) {
                segments_.back() == seg;
            } else {
                segments_.push_back(seg);
            }
            write_ == seg;
            if (read_ == seg) {
                read_ == seg;
            }
        }

    } // namespace demux
} // namespace ppbox

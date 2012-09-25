// BufferList.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/BufferList.h"
#include "ppbox/demux/base/Content.h"
#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceError.h"
#include "ppbox/demux/base/BytesStream.h"

#include <framework/system/LogicError.h>
#include <framework/container/Array.h>
#include <framework/memory/PrivateMemory.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/timer/TimeCounter.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/bind.hpp>

#include <iostream>
#include <fstream>

namespace ppbox
{
    namespace demux
    {

        FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferList", 0)

        BufferList::BufferList(
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size, 
            Content * source,
            BufferDemuxer * demuxer,
            size_t total_req)
            : root_source_(source)
            , demuxer_(demuxer)
            , num_try_(size_t(-1))
            , max_try_(size_t(-1))
            , buffer_(NULL)
            , buffer_size_(framework::memory::MemoryPage::align_page(buffer_size))
            , prepare_size_(prepare_size)
            , time_block_(0)
            , time_out_(0)
            , source_closed_(true)
            , data_beg_(0)
            , data_end_(0)
            , seek_end_(boost::uint64_t(-1))
            , amount_(0)
            , expire_pause_time_(Time::now())
            , total_req_(total_req)
            , sended_req_(0)
        {
            buffer_ = (char *)memory_.alloc_block(buffer_size_);
            read_.buffer = buffer_beg();
            read_.offset = 0;
            read_.segment = 0;
            read_.size_beg = 0;
            write_tmp_ = write_ = read_;
            write_hole_.this_end = boost::uint64_t(-1);
            write_tmp_.buffer = NULL;
            source_init();

            read_bytesstream_ = new BytesStream(
                *this, read_);
            write_bytesstream_ = new BytesStream(
                *this, write_);
        }

        BufferList::~BufferList()
        {
            if (read_bytesstream_)
                delete read_bytesstream_;
            if (write_bytesstream_)
                delete write_bytesstream_;
            if (buffer_)
                memory_.free_block(buffer_, buffer_size_);
        }

        // 目前只发生在，seek到一个分段，还没有该分段头部数据时，
        // 此时size为head_size_头部数据大小
        // TO BE FIXED
        boost::system::error_code BufferList::seek(
            SegmentPositionEx const & abs_position,
            SegmentPositionEx & position,
            boost::uint64_t offset, 
            boost::uint64_t end, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (abs_position_.segment == boost::uint32_t(-1)) {
                abs_position_ = abs_position;
            }
            if ( abs_position != abs_position_ ) {
                abs_position_ = abs_position;
                boost::system::error_code ec1;
                close_segment(ec1, false);
                close_all_request(ec1);

                reset(position, offset);
                return ec;
            }

            offset += position.size_beg;// 绝对偏移量
            if (end != (boost::uint64_t)-1)
                end += position.size_beg;
            assert(end > offset);
            if (offset < read_.offset || offset > write_.offset 
                || end < write_hole_.this_end) {// 读指针之前，或写指针之后，或当前写空洞之间
                    // close source for open from new offset
                    boost::system::error_code ec1;
                    close_segment(ec1, false);
                    close_all_request(ec1);
            }
            seek_to(offset);
            SegmentPositionEx & read = read_;
            read = position;
            SegmentPositionEx & write = write_;
            write = position;
            //root_source_->size_seek(write_.offset, abs_position_, write_, ec);
            if (!ec) {
                if (offset >= seek_end_)
                    seek_end_ = (boost::uint64_t)-1;
                if (end < seek_end_)
                    seek_end_ = end;
            }
            if (source_closed_ || seek_end_ == (boost::uint64_t)-1) {
                update_hole(write_, write_hole_);
                write_tmp_ = write_;
                write_tmp_.buffer = NULL;
                write_hole_tmp_ = write_hole_;
            }
            // 又有数据下载了
            if (!ec && (source_error_ == source_error::no_more_segment
                || source_error_ == source_error::at_end_point))
                source_error_.clear();

            read_bytesstream_->seek(offset);

            return ec;
        }

        // seek到分段的具体位置offset
        // TO BE FIXED
        boost::system::error_code BufferList::seek(
            SegmentPositionEx const & abs_position,
            SegmentPositionEx & position, 
            boost::uint64_t offset, 
            boost::system::error_code & ec)
        {
            boost::system::error_code ret_ec = seek(abs_position, position, offset, (boost::uint64_t)-1, ec);
            return ret_ec;
        }

        void BufferList::pause(
            boost::uint32_t time)
        {
            expire_pause_time_ = Time::now() + Duration::milliseconds(time);
        }

        void BufferList::set_time_out(
            boost::uint32_t time_out)
        {
            time_out_ = time_out / 1000;
        }

        void BufferList::set_max_try(
            size_t max_try)
        {
            max_try_ = max_try;
        }

        boost::system::error_code BufferList::cancel(
            boost::system::error_code & ec)
        {
            source_error_ = boost::asio::error::operation_aborted;
            return write_.source->get_source()->cancel(ec);
        }

        boost::system::error_code BufferList::close(
            boost::system::error_code & ec)
        {
            return close_all_request(ec);
        }

        //************************************
        // Method:    prepare
        // FullName:  ppbox::demux::BufferList::prepare
        // Access:    public 
        // Returns:   boost::system::error_code
        // Qualifier:
        // Parameter: boost::uint32_t amount 需要下载的数据大小
        // Parameter: boost::system::error_code & ec
        //************************************
        boost::system::error_code BufferList::prepare(
            boost::uint32_t amount, 
            boost::system::error_code & ec)
        {
            SegmentPositionEx write_seg = write_;
            ec = source_error_;
            while (1) {
                if (ec) {
                } else if (write_.offset >= write_hole_.this_end) {
                    ec = boost::asio::error::eof;
                } else if (write_.offset >= read_.offset + buffer_size_) {// 写满
                    ec = boost::asio::error::no_buffer_space;
                    break;
                } else if (source_closed_ && open_segment(false, ec)) {
                } else if (write_.source->get_source()->is_open(ec)) {
                    // 请求的分段打开成功，更新 (*segments_) 信息
                    update_segments(ec);
                    framework::timer::TimeCounter tc;
                    size_t bytes_transferred = boost::asio::read(
                        *write_.source->get_source(), 
                        write_buffer(amount),
                        boost::asio::transfer_all(), 
                        ec);
                    if (tc.elapse() > 10) {
                        LOG_DEBUG("[prepare] read elapse: " << tc.elapse() 
                            << " bytes_transferred: " << bytes_transferred);
                    }
                    increase_download_byte(bytes_transferred);
                    move_front(write_, bytes_transferred);
                    if (ec && !write_.source->get_source()->continuable(ec)) {
                        LOG_WARN("[prepare] read_some: " << ec.message() << 
                            " --- failed " << num_try_ << " times");
                        if (ec == boost::asio::error::eof) {
                            LOG_DEBUG("[prepare] read eof, write_.offset: " << write_.offset
                                << " write_hole_.this_end: " << write_hole_.this_end);
                        }
                    }
                    if (data_end_ < write_.offset)
                        data_end_ = write_.offset;
                } else {
                    // 打开失败
                    if (!write_.source->get_source()->continuable(ec)) {
                        LOG_WARN("[prepare] open_segment: " << ec.message() << 
                            " --- failed " << num_try_ << " times");
                        //close_segment(ec);
                    } else {
                        increase_download_byte(0);
                    }
                }
                if (source_error_) {
                    ec = source_error_;
                }
                if (!ec) {
                    break;
                }

                bool is_error = handle_error(ec);
                if (is_error) {
                    if (ec == boost::asio::error::eof) {
                        open_segment(true, ec);
                        if (ec && !handle_error(ec))
                            break;
                    } else {
                        open_segment(false, ec);
                    }
                } else {
                    break;
                }
            }
            // 更新读写分段
            write_bytesstream_->update();
            if (read_.segment == write_.segment)
                read_bytesstream_->update();
            last_ec_ = ec;
            return ec;
        }

        void BufferList::async_prepare(
            boost::uint32_t amount, 
            open_response_type const & resp)
        {
            amount_ = amount;
            resp_ = resp;
            handle_async(boost::system::error_code(), 0);
        }

        void BufferList::handle_async(
            boost::system::error_code const & ecc, 
            size_t bytes_transferred)
        {
            boost::system::error_code ec = ecc;
            bool is_open_callback = false;
            if (bytes_transferred == (size_t)-1) {// 打开的回调
                is_open_callback = true;
                bytes_transferred = 0;
            }
            if (ec && write_.source && !write_.source->get_source()->continuable(ec)) {
                if (is_open_callback) {
                    LOG_DEBUG("[handle_async] open_segment: " << ec.message() << 
                        " --- failed " << num_try_ << " times");
                }
                if (!source_closed_) {
                    LOG_WARN("[handle_async] read_some: " << ec.message() << 
                        " --- failed " << num_try_ << " times");
                    if (ec == boost::asio::error::eof) {
                        LOG_DEBUG("[handle_async] read eof, write_.offset: " << write_.offset
                            << " write_hole_.this_end: " << write_hole_.this_end);
                    }
                }
            }
            if (bytes_transferred > 0) {
                increase_download_byte(bytes_transferred);
                move_front(write_, bytes_transferred);
                if (data_end_ < write_.offset)
                    data_end_ = write_.offset;
                read_bytesstream_->update();
                write_bytesstream_->update();
                if (amount_ <= bytes_transferred) {
                    response(ec);
                    return;
                } else {
                    amount_ -= bytes_transferred;
                }
            }
            if (source_error_) {
                ec = source_error_;
            }
            if (ec) {
                bool is_error = handle_error(ec);
                if (is_error) {
                    if (ec == boost::asio::error::eof) {
                        reset_zero_interval();
                        time_block_ = 0;
                        async_open_segment(true, boost::bind(&BufferList::handle_async, this, _1, (size_t)-1));
                        return;
                    } else {
                        async_open_segment(false, boost::bind(&BufferList::handle_async, this, _1, (size_t)-1));
                        return;
                    }
                } else {
                    boost::system::error_code ec1;
                    close_request(ec1);
                }
            } else if (write_.offset >= write_hole_.this_end) {
                ec = boost::asio::error::eof;
                return handle_async(ec, 0);
            } else if (write_.offset >= read_.offset + buffer_size_) {
                ec = boost::asio::error::no_buffer_space;
            } else if (source_closed_) {
                async_open_segment(false, boost::bind(&BufferList::handle_async, this, _1, (size_t)-1));
                return;
            } else {
                update_segments(ec);
                boost::asio::async_read(
                    *write_.source->get_source(), 
                    write_buffer(amount_),
                    boost::asio::transfer_all(), 
                    boost::bind(&BufferList::handle_async, this, _1, _2));
                return;
            }

            response(ec);
        }

        void BufferList::response(
            boost::system::error_code const & ec)
        {
            write_tmp_ = write_;
            write_tmp_.buffer = NULL;
            write_hole_tmp_ = write_hole_;
            boost::system::error_code ecc = boost::system::error_code();
            open_request(true, ecc);
            open_response_type resp;
            resp.swap(resp_);

            last_ec_ = ec;
            resp(ec, 0);
        }

        boost::system::error_code BufferList::prepare_at_least(
            boost::uint32_t amount, 
            boost::system::error_code & ec)
        {
            return prepare(amount < prepare_size_ ? prepare_size_ : amount, ec);
        }

        void BufferList::async_prepare_at_least(
            boost::uint32_t amount, 
            open_response_type const & resp)
        {
            async_prepare(amount < prepare_size_ ? prepare_size_ : amount, resp);
        }

        boost::system::error_code BufferList::peek(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            std::vector<unsigned char> & data, 
            boost::system::error_code & ec)
        {
            offset += read_.size_beg;
            assert(offset >= read_.offset && offset + size <= read_.size_end);
            if (offset < read_.offset) {
                ec = framework::system::logic_error::out_of_range;
            } else if (offset + size > read_.size_end) {
                ec = boost::asio::error::eof;
            } else {
                if (offset + size <= write_.offset) {
                    prepare_at_least(0, ec);
                } else {
                    prepare_at_least((boost::uint32_t)(offset + size - write_.offset), ec);
                }
                if (offset + size <= write_.offset) {
                    data.resize(size);
                    read(offset, size, &data.front());
                    ec = boost::system::error_code();
                }
            }

            //last_ec_ = ec;
            return ec;
        }

        boost::system::error_code BufferList::peek(
            boost::uint32_t size, 
            std::vector<unsigned char> & data, 
            boost::system::error_code & ec)
        {
            return peek(read_.offset - read_.size_beg, size, data, ec);
        }

        //************************************
        // Method:    peek
        // FullName:  ppbox::demux::BufferList::peek
        // Access:    public 
        // Returns:   boost::system::error_code
        // Qualifier:
        // Parameter: boost::uint64_t offset 读分段的相对偏移量
        // Parameter: boost::uint32_t size 大小
        // Parameter: std::deque<boost::asio::const_buffer> & data 输出缓存
        // Parameter: boost::system::error_code & ec 错误码
        //************************************
        boost::system::error_code BufferList::peek(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            std::deque<boost::asio::const_buffer> & data, 
            boost::system::error_code & ec)
        {
            offset += read_.size_beg;
            assert(offset >= read_.offset && offset + size <= read_.size_end);
            if (offset < read_.offset) {
                ec = framework::system::logic_error::out_of_range;
            } else if (offset + size > read_.size_end) {
                ec = boost::asio::error::eof;
            } else {
                if (offset + size <= write_.offset) {// 是否超出当前写指针
                    prepare_at_least(0, ec);
                } else {
                    prepare_at_least((boost::uint32_t)(offset + size - write_.offset), ec);
                }
                if (offset + size <= write_.offset) {
                    read(offset, size, data);
                    ec = boost::system::error_code();
                }
            }
            //last_ec_ = ec;
            return ec;
        }

        boost::system::error_code BufferList::peek(
            boost::uint32_t size, 
            std::deque<boost::asio::const_buffer> & data, 
            boost::system::error_code & ec)
        {
            return peek(read_.offset - read_.size_beg, size, data, ec);
        }

        boost::system::error_code BufferList::read(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            std::vector<unsigned char> & data, 
            boost::system::error_code & ec)
        {
            peek(offset, size, data, ec);
            offset += read_.size_beg;
            if (ec) {
                boost::uint64_t drop_offset = offset;
                if (drop_offset > write_.offset) {
                    drop_offset = write_.offset;
                }
                move_front_to(read_, drop_offset);
            } else {
                move_front_to(read_, offset + size);
            }
            return ec;
        }

        boost::system::error_code BufferList::read(
            boost::uint32_t size, 
            std::vector<unsigned char> & data, 
            boost::system::error_code & ec)
        {
            return read(read_.offset - read_.size_beg, size, data, ec);
        }

        boost::system::error_code BufferList::drop(
            boost::system::error_code & ec)
        {
            boost::uint32_t pos = read_bytesstream_->position();
            boost::system::error_code ret_ec = read_seek_to(read_.size_beg + pos, ec);
            read_bytesstream_->drop();
            if (write_.segment == write_.segment)
                write_bytesstream_->drop();
            return ret_ec;
        }

        boost::system::error_code BufferList::drop_to(
            boost::uint64_t offset, 
            boost::system::error_code & ec)
        {
            if (read_.size_beg + offset < read_.offset) {
                ec = framework::system::logic_error::invalid_argument;
            } else {
                read_seek_to(read_.size_beg + offset, ec);
            }
            return ec;
        }

        /**
        drop_all 
        丢弃当前分段的所有剩余数据，并且更新当前分段信息
        */
        // TO BE FIXED
        boost::system::error_code BufferList::drop_all(
            boost::system::error_code & ec)
        {
            if (read_.total_state < SegmentPositionEx::is_valid) {
                assert(read_.segment == write_.segment);
                write_.shard_end = write_.size_end = write_.offset;
                read_.shard_end = read_.size_end = write_.offset;
                read_.total_state = SegmentPositionEx::by_guess;
                LOG_INFO("[drop_all] guess segment size " << read_.size_end - read_.size_beg);
            }
            read_seek_to(read_.shard_end, ec);
            if (!ec) {
                read_.source->next_segment(read_);
                if (read_.total_state < SegmentPositionEx::is_valid
                    && read_.segment == write_.segment) {
                        read_.size_end = write_.size_end;
                        read_.shard_end = write_.shard_end;
                        read_.total_state = write_.total_state;
                }
            }

            // 读缓冲DropAll
            read_bytesstream_->drop_all();

            return ec;
        }

        void BufferList::clear()
        {
            //segments_->clear();
            read_ = PositionEx();
            read_.buffer = buffer_beg();
            write_tmp_ = write_ = read_;
            write_tmp_.buffer = NULL;

            read_hole_.this_end = 0;
            read_hole_.next_beg = 0;
            write_hole_tmp_ = write_hole_ = read_hole_;

            time_block_ = 0;
            time_out_ = 0;

            source_closed_ = true;
            data_beg_ = 0;
            data_end_ = 0;
            seek_end_ = (boost::uint64_t)-1;
            amount_ = 0;
            expire_pause_time_ = Time::now();
            sended_req_ = 0;
            clear_error();

            read_bytesstream_->close();
            write_bytesstream_->close();
        }

        void BufferList::reset(SegmentPositionEx const & seg, boost::uint32_t offset)
        {
            num_try_ = 0;
            time_block_ = 0;
            time_out_ = 0;
            source_closed_ = true;
            data_beg_ = 0;
            data_end_ = 0;
            seek_end_ = boost::uint64_t(-1);
            amount_ = 0;
            expire_pause_time_ = Time::now();
            sended_req_ = 0;

            write_.buffer = buffer_beg();
            write_.offset = seg.size_beg + offset;
            write_.segment = seg.segment;
            write_.time_beg = seg.time_beg;
            write_.time_end = seg.time_end;
            write_.size_beg = 0;
            write_.size_end = seg.size_end;
            write_.shard_beg = seg.shard_beg;
            write_.shard_end = seg.shard_end;
            write_.total_state = seg.total_state;
            write_hole_.this_end = write_hole_.next_beg = boost::uint64_t(-1);
            write_hole_tmp_ = write_hole_;
            write_.source = root_source_;

            write_tmp_ = read_ = write_;
            write_tmp_.buffer = NULL;

            delete read_bytesstream_;
            read_bytesstream_ = new BytesStream(
                *this, read_);
            delete write_bytesstream_;
            write_bytesstream_ = new BytesStream(
                *this, write_);

            //read_.offset = offset;
            //read_.buffer = buffer_beg();
            //write_.offset = offset;
            //write_.buffer = buffer_beg();
            //data_beg_ = data_end_ = offset;
            //write_hole_.this_end = write_.offset;
            //write_hole_.next_beg = boost::uint64_t(-1);
            //read_hole_.next_beg = offset;
            //read_.segment = seg.segment;
            //write_.segment = seg.segment;

            //write_hole_tmp_ = write_hole_;

            ////read_bytesstream_->do_close();
            ////read_bytesstream_->do_update_new(read_);
            ////write_bytesstream_->do_close();
            ////write_bytesstream_->do_update_new(write_);
            //delete read_bytesstream_;
            //read_bytesstream_ = new BytesStream(*this, read_);
            //delete write_bytesstream_;
            //write_bytesstream_ = new BytesStream(*this, write_);

            clear_error();
        }

        void BufferList::update_abs_segment(
            SegmentPositionEx const & seg, boost::uint32_t offset)
        {
            if (seg == abs_position_) return;
            abs_position_   = seg;
            
            read_.size_beg      -= offset;
            read_.shard_beg     -= offset;
            read_.size_beg      -= offset;
            read_.size_end      -= offset;

            write_.size_beg     -= offset;
            write_.shard_beg    -= offset;
            write_.size_end     -= offset;
            write_.shard_end    -= offset;

            write_tmp_ = write_;
            
        }

        // 当前读分段读指针之前的大小
        boost::uint64_t BufferList::read_front() const
        {
            return segment_read_front(read_);
        }

        // 当前读分段写指针之前的大小
        boost::uint64_t BufferList::read_back() const
        {
            return segment_read_back(read_);
        }

        // 获取指定分段读指针之前的大小
        boost::uint64_t BufferList::segment_read_front(
            SegmentPositionEx const & segment) const
        {
            if (read_.offset <= segment.size_beg) {
                return 0;
            } else if (read_.offset < segment.size_end) {
                return read_.offset - segment.size_beg;
            } else {
                return segment.size_end - segment.size_beg;
            }
        }

        // 获取指定分段写指针之前的大小
        boost::uint64_t BufferList::segment_read_back(
            SegmentPositionEx const & segment) const
        {
            if (write_.offset <= segment.size_beg) {
                return 0;
            } else if (write_.offset < segment.size_end) {
                return write_.offset - segment.size_beg;
            } else {
                return segment.size_end - segment.size_beg;
            }
        }

        boost::system::error_code BufferList::segment_buffer(
            SegmentPositionEx const & segment, 
            PositionType::Enum pos_type, 
            boost::uint64_t & pos, 
            boost::uint32_t & off, 
            boost::asio::const_buffer & buffer)
        {
            boost::uint64_t beg = segment.shard_beg;
            boost::uint64_t end = segment.shard_end;
            if (beg < read_.offset) {
                beg = read_.offset;
            }
            if (end > write_.offset) {
                end = write_.offset;
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
                pos += segment.size_beg;
                if (pos < beg) {
                   pos = beg;
                   ec = framework::system::logic_error::out_of_range;
                } else if (pos > end) {
                    if (pos > segment.shard_end) {
                        pos = segment.shard_end;
                        ec = framework::system::logic_error::out_of_range;
                    }
                    if (pos > write_.offset) {
                    boost::system::error_code ec1 = last_ec_;;
                        ec1 || prepare_at_least(pos - write_.offset, ec1);
                        if (pos > write_.offset) {
                            pos = write_.offset;
                            if (!ec) ec = ec1;
                        }
                        end = segment.shard_end;
                        if (end > write_.offset)
                            end = write_.offset;
                    }
                    assert(pos <= end);
                }
            }
            char const * ptr = buffer_move_front(read_.buffer, beg - read_.offset);
            boost::uint64_t buf_end = beg + (boost::uint32_t)(buffer_end() - ptr);
            if (pos < buf_end) {
                if (end > buf_end)
                    end = buf_end;
            } else {
                ptr = buffer_beg();
                beg = buf_end;
            }
            off = pos - beg;
            pos = beg - segment.size_beg;
            buffer = boost::asio::const_buffer(ptr, end - beg);
            return ec;
        }

        // 当前所有写缓冲
        BufferList::write_buffer_t BufferList::write_buffer()
        {
            boost::uint64_t beg = write_.offset;
            boost::uint64_t end = read_.offset + buffer_size_;
            if (end > write_hole_.this_end)
                end = write_hole_.this_end;
            return write_buffer(beg, end);
        }

        // 获取写缓冲区
        // prepare下载使用
        BufferList::write_buffer_t BufferList::write_buffer(
            boost::uint32_t max_size)
        {
            boost::uint64_t beg = write_.offset;
            boost::uint64_t end = read_.offset + buffer_size_;
            if (end > write_hole_.this_end)
                end = write_hole_.this_end;
            if (end > beg + max_size)
                end = beg + max_size;
            return write_buffer(beg, end);
        }

        //************************************
        // Method:    add_request 串行请求
        // FullName:  ppbox::demux::BufferList::add_request
        // Access:    public 
        // Returns:   void
        // Qualifier:
        // Parameter: boost::system::error_code & ec
        //************************************
        void BufferList::add_request(
            boost::system::error_code & ec)
        {
            if (sended_req_ && resp_.empty()) {
                open_request(true, ec);
            }
        }

        void BufferList::source_init()
        {
            root_source_->reset(write_);
            write_hole_.this_end = write_hole_.next_beg = write_.size_end;
            write_.source = root_source_;
            read_ = write_;
            abs_position_ = read_;
        }

        void BufferList::insert_source(
            boost::uint64_t offset,
            Content * source, 
            boost::uint64_t size,
            boost::system::error_code & ec)
        {
            boost::uint64_t abs_offset = write_.size_beg + offset;
            if (abs_offset <= read_.offset) {// 插入分段在当前读位置之前
                data_beg_ = abs_offset + size;
                read_.offset += size;
                read_.shard_beg = read_.size_beg += size;
                read_.shard_end = read_.size_end += size;
                read_hole_.next_beg += size;
                write_.offset += size;
                write_.shard_beg = write_.size_beg += size;
                write_.shard_end = write_.size_end += size;
                write_hole_.next_beg += size;
                write_hole_.this_end += size;
                write_tmp_.offset += size;
                write_tmp_.shard_beg = write_tmp_.size_beg += size;
                write_tmp_.shard_end = write_tmp_.size_end += size;
                write_hole_tmp_.next_beg += size;
                write_hole_tmp_.this_end += size;
            } else if (abs_offset <= write_.offset) {// 插入分段在当前写位置之前
                close_segment(ec, false);
                close_all_request(ec);
                move_back_to(write_, abs_offset);
                write_tmp_ = write_;
                write_tmp_.buffer = NULL;
                write_hole_.next_beg = write_hole_.this_end = abs_offset;
                write_hole_tmp_ = write_hole_;
                data_end_ = abs_offset;
            } else if (abs_offset < write_hole_.this_end) {// 插入位置在当前写空洞之前
                if (sended_req_ > 1) {
                    close_segment(ec, false);
                    close_all_request(ec);
                }
                write_hole_.next_beg = write_hole_.this_end = abs_offset;
                write_hole_tmp_ = write_hole_;
                data_end_ = write_.offset;
            } else {
                if (abs_offset < write_hole_tmp_.this_end) {
                    close_segment(ec, false);
                    close_all_request(ec);
                }
                if (abs_offset < data_end_)
                    data_end_ = abs_offset;
            }
            // 更新读写分段
            read_.source->size_seek(read_.offset, abs_position_, read_, ec);
            write_.source->size_seek(write_.offset, abs_position_, write_, ec);
            write_tmp_ = write_;
            write_tmp_.buffer = NULL;
        }

        // 返回false表示不能再继续了
        bool BufferList::handle_error(
            boost::system::error_code& ec)
        {
            if (write_.source->get_source()->continuable(ec)) {
                time_block_ = get_zero_interval();
                if (time_out_ > 0 && time_block_ > time_out_) {
                    LOG_WARN("source.read_some: timeout" << 
                        " --- failed " << num_try_ << " times");
                    ec = boost::asio::error::timed_out;
                    if (can_retry()) {
                        return true;
                    }
                } else {
                    return false;
                }
            } else if (ec == boost::asio::error::eof) {
                if (write_.offset >= write_hole_.this_end) {
                    return true;
                } else if (write_.total_state == SegmentPositionEx::not_exist) {
                    write_.total_state = SegmentPositionEx::by_guess;
                    write_.shard_end = write_.size_end = write_.offset;
                    write_hole_.this_end = write_.offset;
                    if (read_.segment == write_.segment) {
                        read_.shard_end = read_.size_end = write_.size_end;
                        read_.total_state = SegmentPositionEx::by_guess;
                    }
                    if (write_tmp_.segment == write_.segment) {
                        write_tmp_.shard_end = write_tmp_.size_end = write_.size_end;
                        write_tmp_.total_state = SegmentPositionEx::by_guess;
                    }
                    LOG_INFO("[handle_error] guess segment size " << write_.size_end - write_.size_beg);
                    return true;
                } else if (can_retry()) {
                    ec = boost::asio::error::connection_aborted;
                    return true;
                }
            } else if(write_.source->get_source()->recoverable(ec)) {
                if (can_retry()) {
                    return true;
                }
            }
            write_.source->on_error(ec);
            if (ec) {
                demuxer_->on_error(ec);
            }
            if (ec)
                source_error_ = ec;
            return !ec;
        }

        void BufferList::seek_to(
            boost::uint64_t offset)
        {
            LOG_TRACE("seek_to " << offset);
            if (data_end_ > data_beg_ + buffer_size_)
                data_beg_ = data_end_ - buffer_size_;// 调整data_beg_
            dump();
            if (offset + buffer_size_ <= data_beg_ || data_end_ + buffer_size_ <= offset) {
                read_.offset = offset;
                read_.buffer = buffer_beg();
                write_.offset = offset;
                write_.buffer = buffer_beg();
                data_beg_ = data_end_ = offset;
                write_hole_.this_end = write_.offset;
                write_hole_.next_beg = boost::uint64_t(-1);
                read_hole_.next_beg = offset;
            } else if (offset < read_.offset) {
                // e    b-^--e    b----R----Wb    e-----E
                move_back_to(read_, read_read_hole(read_hole_.next_beg, read_hole_));
                // e    b-^--e    bR--------Wb    e-----E
                while (read_hole_.this_end > offset) {
                    // e    b-^----e    bR-------Wb   e-----E
                    // 有可能两个写空洞合并
                    if (read_.offset < write_.offset) {
                        // lay a write hole
                        write_hole_.next_beg = write_write_hole(write_.offset, write_hole_);
                        write_hole_.this_end = read_.offset;
                    }
                    move_back_to(write_, read_hole_.this_end);
                    move_back_to(read_, read_read_hole(read_hole_.next_beg, read_hole_));
                    // e    bR-^--Wb    e---------b   e-----E
                }
                // e    b--R----Wb    e--------b    e-----E
                // |   offset   |
                boost::uint64_t read_hole_next_beg = read_.offset;
                if (offset >= read_.offset) {
                    //  e         b--R--^-Wb    e-----E
                    move_front_to(read_, offset);
                } else {
                    //  e   ^     b--R----Wb    e-----E
                    // 有可能两个写空洞合并
                    if (read_.offset != write_.offset) {
                        // lay a write hole
                        write_hole_.next_beg = write_write_hole(write_.offset, write_hole_);
                        write_hole_.this_end = read_.offset;
                    }
                    if (data_beg_ > offset) {
                        // 两步跳，防止跨度过大
                        move_back_to(read_, data_beg_);
                        data_beg_ = offset;
                        if (data_end_ > data_beg_ + buffer_size_)
                            data_end_ = data_beg_ + buffer_size_;
                        LOG_TRACE("backward data: " << data_beg_ << "-" << data_end_);
                    }
                    move_back_to(read_, offset);
                    read_hole_next_beg = offset;
                    write_ = read_;
                }
                // lay a read hole
                read_hole_.next_beg = write_read_hole(read_hole_next_beg, read_hole_);
            } else {
                // e    b----e    b--R--Wb    e----b    e-----E
                move_back_to(read_, read_read_hole(read_hole_.next_beg, read_hole_));
                // e    b----e    bR----Wb    e----b    e-----E
                // e    b----e    b------e    bR---W    e-----E
                while (write_hole_.this_end < offset) {
                    if (data_end_ < write_hole_.this_end) {
                        data_end_ = write_hole_.this_end;
                        if (data_end_ > data_beg_ + buffer_size_)
                            data_beg_ = data_end_ - buffer_size_;
                        LOG_TRACE("advance data: " << data_beg_ << "-" << data_end_);
                    }
                    // 有可能两个读空洞合并
                    if (read_.offset < write_.offset) {
                        // lay a read hole
                        read_hole_.next_beg = write_read_hole(read_.offset, read_hole_);
                        read_hole_.this_end = write_.offset;
                    }
                    move_front_to(read_, write_hole_.this_end);
                    move_front_to(write_, read_write_hole(write_hole_.next_beg, write_hole_));
                }
                boost::uint64_t read_hole_next_beg = read_.offset;
                if (offset <= write_.offset) {
                    //           R--^-W    e-----E
                    move_front_to(read_, offset);
                } else {
                    //           R----W  ^ e-----E
                    // 有可能两个读空洞合并
                    if (read_.offset < write_.offset) {
                        read_hole_.next_beg = write_read_hole(read_.offset, read_hole_);
                        read_hole_.this_end = write_.offset;
                    }
                    if (data_end_ < offset) {
                        // 两步跳，防止跨度过大
                        move_front_to(write_, data_end_);
                        data_end_ = offset;
                        if (data_end_ > data_beg_ + buffer_size_)
                            data_beg_ = data_end_ - buffer_size_;
                        LOG_TRACE("advance data: " << data_beg_ << "-" << data_end_);
                    }
                    move_front_to(write_, offset);
                    read_ = write_;
                    read_hole_next_beg = offset;
                }
                // lay a read hole
                read_hole_.next_beg = write_read_hole(read_hole_next_beg, read_hole_);
            }
            LOG_TRACE("after seek_to " << offset);
            dump();
        }

        /**
        只在当前分段移动read指针，不改变write指针
        只能在当前分段内移动
        即使移动到当前段的末尾，也不可以改变read内的当前分段
        */
        boost::system::error_code BufferList::read_seek_to(
            boost::uint64_t offset, 
            boost::system::error_code & ec)
        {
            assert(offset >= read_.offset && offset <= read_.size_end);
            if (offset < read_.offset) {
                ec = framework::system::logic_error::out_of_range;
            } else if (offset > read_.size_end) {
                ec = boost::asio::error::eof;
            } else if (offset <= write_.offset) {
                move_front_to(read_, offset);
                ec = boost::system::error_code();
            } else { // offset > write_.offset
                prepare((boost::uint32_t)(offset - write_.offset), ec);
                if (offset <= write_.offset) {
                    move_front_to(read_, offset);
                    ec = boost::system::error_code();
                }
            }
            return ec;
        }

        boost::system::error_code BufferList::next_write_hole(
            PositionEx & pos, 
            Hole & hole, 
            boost::system::error_code & ec)
        {
            ec.clear();
            boost::uint64_t next_offset = read_write_hole(hole.next_beg, hole);

            if (next_offset >= seek_end_) {
                return ec = source_error::at_end_point;
            }

            if (next_offset >= pos.shard_end) {
                std::cout << "pos.size_end = " << pos.size_end << std::endl;
                pos.source->next_segment(pos);
                if (!pos.source) {
                    return ec = source_error::no_more_segment;
                }
            }

            if (pos.buffer != NULL) {
                move_front_to(pos, next_offset);
            } else {
                pos.offset = next_offset;
            }

            update_hole(pos, hole);
            return ec;
        }

        void BufferList::update_hole(
            PositionEx & pos,
            Hole & hole)
        {
            // W     e^b    e----b      e---
            // 如果当这个分段不能完全填充当前空洞，会切分出一个小空洞，需要插入
            boost::uint64_t end = pos.shard_end;
            if (end > seek_end_) {   // 一般这种可能性是先下头部数据的需求
                end = seek_end_;
            }
            if (end < hole.this_end) {
                if (write_write_hole(end, hole) == boost::uint64_t(-1))
                    data_end_ = pos.offset;
                hole.this_end = end;
                hole.next_beg = end;
            }
        }

        void BufferList::update_segments(
            boost::system::error_code & ec)
        {
            if (write_.total_state != SegmentPositionEx::is_valid) {
                boost::uint64_t file_length = write_.source->get_source()->total(ec);
                if (ec) {
                    file_length = boost::uint64_t(-1);
                    write_.total_state = SegmentPositionEx::not_exist;
                } else {
                    write_.total_state = SegmentPositionEx::is_valid;
                    write_.shard_end = write_.size_end = write_.size_beg + file_length;
                    if (write_hole_.this_end >= write_.size_end)
                        write_hole_.this_end = write_.size_end;
                    if (read_.segment == write_.segment) {
                        read_.shard_end = read_.size_end = write_.size_end;
                        read_.total_state = SegmentPositionEx::is_valid;
                    }
                    if (write_tmp_.segment == write_.segment) {
                        write_tmp_.shard_end = write_tmp_.size_end = write_.size_end;
                        write_tmp_.total_state = SegmentPositionEx::is_valid;
                    }
                }
            }
        }

        boost::system::error_code BufferList::open_request(
            bool is_next_segment,
            boost::system::error_code & ec)
        {
            for (; sended_req_ < total_req_; ) {
                PositionEx write_tmp = write_tmp_;
                Hole write_hole_tmp = write_hole_tmp_;
                if (is_next_segment) {
                    boost::uint64_t data_end_tmp = data_end_;
                    if (data_end_ < write_hole_tmp_.this_end 
                        && write_hole_tmp_.this_end <= write_tmp_.size_end 
                        && write_hole_tmp_.this_end != boost::uint64_t(-1)) {
                            data_end_ = write_hole_tmp_.this_end;
                    }
                    if (next_write_hole(write_tmp_, write_hole_tmp_, ec)) {
                        if (sended_req_ && ec == source_error::no_more_segment) {
                            ec.clear();
                        }
                        data_end_ = data_end_tmp;
                        break;
                    }
                    data_end_ = data_end_tmp;
                }
                LOG_TRACE("[open_request] segment: " << write_tmp_.segment << " sended_req: " << sended_req_ << "/" << total_req_);

                size_t total_count = write_tmp_.source->get_media()->segment_count();
                if (total_count == (size_t)-1 || write_tmp_.segment < total_count) {
                    ++sended_req_;
                    std::string url;
                    boost::uint64_t from = write_tmp_.offset - write_tmp_.size_beg;
                    boost::uint64_t to = write_hole_tmp_.this_end == boost::uint64_t(-1) || write_hole_tmp_.this_end == write_tmp_.size_end ? 
                        boost::uint64_t(-1) : write_hole_tmp_.this_end - write_tmp_.size_beg;
                    framework::string::Url url_temp(url);
                    write_tmp_.source->get_media()->segment_url(write_tmp_.segment, url_temp, ec);
                    write_tmp_.source->get_source()->open(/*write_tmp_.segment,*/ url_temp, from , to, ec);
                } else {
                    LOG_TRACE("[open_request] this is the last segment: " << write_tmp_.segment);
                    ec = boost::asio::error::not_found;
                }

                if (ec) {
                    if (write_tmp_.source->get_source()->continuable(ec)) {
                        if (sended_req_) // 如果已经发过一个请求
                            ec.clear();
                    } else {
                        write_tmp_ = write_tmp;
                        write_hole_tmp_ = write_hole_tmp;
                        break;
                    }
                }
                is_next_segment = true;
            }
            return ec;
        }

        boost::system::error_code BufferList::close_request(
            boost::system::error_code & ec)
        {
            if (sended_req_) {
                write_.source->get_source()->close(ec);
                --sended_req_;
                LOG_TRACE("[close_request] segment: " << write_.segment << " sended_req: " << sended_req_ << "/" << total_req_);
            } else {
                return ec;
            }

            return ec;
        }

        boost::system::error_code BufferList::close_all_request(
            boost::system::error_code & ec)
        {
            write_tmp_ = write_;
            write_tmp_.buffer = NULL;
            write_hole_tmp_ = write_hole_;
            for (size_t i = 0; i < sended_req_; ++i) {
                write_.source->get_source()->close(ec);
                --sended_req_;
                LOG_TRACE("[close_all_request] segment: " << write_.segment << " sended_req: " << sended_req_ << "/" << total_req_);
                boost::uint64_t data_end_tmp = data_end_;
                if (data_end_ < write_hole_tmp_.this_end 
                    && write_hole_tmp_.this_end <= write_tmp_.size_end 
                    && write_hole_tmp_.this_end != boost::uint64_t(-1)) {
                        data_end_ = write_hole_tmp_.this_end;
                }
                next_write_hole(write_tmp_, write_hole_tmp_, ec);
                data_end_ = data_end_tmp;
            }

            write_tmp_ = write_;
            write_tmp_.buffer = NULL;
            write_hole_tmp_ = write_hole_;

            return ec;
        }

        boost::system::error_code BufferList::open_segment(
            bool is_next_hole, 
            boost::system::error_code & ec)
        {
            close_segment(ec, is_next_hole);

            if (is_next_hole) {
                reset_zero_interval();
                time_block_ = 0;
                close_request(ec);
                num_try_ = 0;
            } else {
                reset_zero_interval();
                close_all_request(ec);
            }

            if (Time::now() < expire_pause_time_) {
                ec = boost::asio::error::would_block;
                return ec;
            }

            open_request(is_next_hole, ec);

            if (ec && !write_.source->get_source()->continuable(ec)) {
                if (!ec)
                    ec = boost::asio::error::would_block;
                LOG_DEBUG("[open_segment] source().open_segment: " << ec.message() << 
                    " --- failed " << num_try_ << " times");
                return ec;
            }

            if (is_next_hole) {
                if (next_write_hole(write_, write_hole_, ec)) {
                    assert(0);
                    return ec;
                }
                write_bytesstream_->drop_all();
            }

            LOG_WARN("[open_segment] write_.segment: " << write_.segment << 
                " write_.offset: " << write_.offset << 
                " begin: " << write_.offset - write_.size_beg << 
                " end: " << write_hole_.this_end - write_.size_beg);

            if (write_.offset - write_.size_beg) {
//                root_source_->get_segment()->segment_size(write_.segment);
            }

            // 分段打开事件通知
            {
                Event evt(Event::EVENT_SEG_DL_OPEN, write_, boost::system::error_code());
                boost::system::error_code last_error = last_ec_;
                last_ec_ = boost::asio::error::would_block;
                write_.source->on_event(evt);
                demuxer_->on_event(evt);
                last_ec_ = last_error;
            }

            source_closed_ = false;

            return ec;
        }

        void BufferList::async_open_segment(
            bool is_next_segment, 
            open_response_type const & resp)
        {
            boost::system::error_code ec;

            close_segment(ec, is_next_segment);

            if (is_next_segment) {
                close_request(ec);
                if (next_write_hole(write_, write_hole_, ec)) {
                    resp(ec, 0);
                    return;
                }
                num_try_ = 0;
            } else {
                reset_zero_interval();
                close_request(ec);
            }

            source_closed_ = false;
            sended_req_++;
            ++num_try_;

            // 分段打开事件通知
            {
                Event evt(Event::EVENT_SEG_DL_OPEN, write_, boost::system::error_code());
                boost::system::error_code last_error = last_ec_;
                last_ec_ = boost::asio::error::would_block;
                write_.source->on_event(evt);
                demuxer_->on_event(evt);
                last_ec_ = last_error;
            }

            std::string url;
            boost::uint64_t from = write_.offset - write_.size_beg;
            boost::uint64_t to = write_hole_.this_end == boost::uint64_t(-1) || write_hole_.this_end == write_.size_end ? 
                boost::uint64_t(-1) : write_hole_.this_end - write_.size_beg;
            framework::string::Url url_temp(url);
            write_.source->get_media()->segment_url(write_.segment, url_temp, ec);
            write_.source->get_source()->async_open(
                /*write_.segment, */
                url_temp, 
                from, 
                to, 
                boost::bind(resp, _1, 0));
        }

        boost::system::error_code BufferList::close_segment(
            boost::system::error_code & ec, bool need_update)
        {
            if (!source_closed_) {
                LOG_DEBUG("[close_segment] write_.segment: " << write_.segment << 
                    " write_.offset: " << write_.offset << 
                    " end: " << write_hole_.this_end - write_.size_beg);

                // 下载结束事件通知
                // write_.size_end = write_.offset;
                if (need_update)
                {
                    Event evt(Event::EVENT_SEG_DL_END, write_, boost::system::error_code());
                    boost::system::error_code last_error = last_ec_;
                    last_ec_ = boost::asio::error::would_block;
                    write_.source->on_event(evt);
                    demuxer_->on_event(evt);
                    last_ec_ = last_error;
                }

                //.write_.source->on_seg_end(write_.segment);
                source_closed_ = true;
            }
            return ec;
        }

        void BufferList::dump()
        {
            LOG_TRACE("buffer:" << (void *)buffer_beg() << "-" << (void *)buffer_end());
            LOG_TRACE("data:" << data_beg_ << "-" << data_end_);
            LOG_TRACE("read:" << read_);
            LOG_TRACE("write:" << write_);
            boost::uint64_t offset = read_hole_.next_beg;
            Hole hole;
            offset = read_read_hole(offset, hole);
            while (1) {
                LOG_TRACE("read_hole:" << offset << "-" << hole.this_end);
                if (hole.this_end == 0)
                    break;
                offset = read_read_hole(hole.next_beg, hole);
            }
            hole = write_hole_;
            offset = write_.offset;
            while (1) {
                LOG_TRACE("write_hole:" << offset << "-" << hole.this_end);
                if (hole.next_beg == boost::uint64_t(-1))
                    break;
                offset = read_write_hole(hole.next_beg, hole);
            }
        }

        boost::uint64_t BufferList::read_write_hole(
            boost::uint64_t offset, 
            Hole & hole) const
        {
            if (offset > data_end_) {
                // next_beg 失效，实际空洞从data_end_开始
                hole.this_end = hole.next_beg = boost::uint64_t(-1);
                return data_end_;
            } else if (offset + sizeof(hole) > data_end_) {
                // 下一个Hole不可读
                hole.this_end = hole.next_beg = boost::uint64_t(-1);
                return offset;
            } else {
                read(offset, sizeof(hole), &hole);
                if (hole.this_end > data_end_) {
                    hole.this_end = hole.next_beg = boost::uint64_t(-1);
                }
                assert(hole.next_beg >= hole.this_end);
                if (hole.this_end != boost::uint64_t(-1))
                    hole.this_end += offset;
                if (hole.next_beg != boost::uint64_t(-1))
                    hole.next_beg += offset;
                return offset;
            }
        }

        boost::uint64_t BufferList::write_write_hole(
            boost::uint64_t offset, 
            Hole hole)
        {
            if (offset + sizeof(hole) < data_end_) {
                if (offset + sizeof(hole) <= hole.this_end) {
                    // 如果空洞较大，可以容纳空洞描述，那么一切正常，当然还要判断不会超过data_end_（在后面处理的）
                } else if (offset + sizeof(hole) < hole.next_beg) {
                    // 如果这个空洞太小，但是下一个空洞还比较远，那么丢弃一部分数据，向后扩张空洞
                    hole.this_end = offset + sizeof(hole);
                } else {
                    // 如果这个空洞太小，而且下一个空洞紧接在后面，那么合并两个空洞
                    read_write_hole(hole.next_beg, hole);
                }
                if (hole.this_end != boost::uint64_t(-1))
                    hole.this_end -= offset;
                if (hole.next_beg != boost::uint64_t(-1))
                    hole.next_beg -= offset;
                // 可以正常插入
                write(offset, sizeof(hole), &hole);
                return offset;
            } else {
                // 没有下一个空洞
                return boost::uint64_t(-1);
            }
        }

        boost::uint64_t BufferList::read_read_hole(
            boost::uint64_t offset, 
            Hole & hole) const
        {
            if (offset < data_beg_) {
                // 下一个Hole不可读
                hole.this_end = 0;
                hole.next_beg = 0;
                return data_beg_;
            } else if (offset < data_beg_ + sizeof(hole)) {
                hole.this_end = 0;
                hole.next_beg = 0;
                return offset;
            } else {
                back_read(offset - sizeof(hole), sizeof(hole), &hole);
                if (hole.this_end < data_beg_) {
                    hole.this_end = 0;
                    hole.next_beg = 0;
                }
                assert(hole.next_beg <= hole.this_end);
                if (hole.this_end != 0)
                    hole.this_end = offset - hole.this_end;
                if (hole.next_beg != 0)
                    hole.next_beg = offset - hole.next_beg;
                return offset;
            }
        }

        boost::uint64_t BufferList::write_read_hole(
            boost::uint64_t offset, 
            Hole hole)
        {
            if (offset > data_beg_ + sizeof(hole)) {
                if (offset > hole.this_end + sizeof(hole)) {
                    // 如果空洞较大，可以容纳空洞描述，那么一切正常，当然还要判断不会超过data_beg_（在后面处理的）
                } else if (offset >= hole.next_beg + sizeof(hole)) {
                    // 如果这个空洞太小，但是下一个空洞还比较远，那么丢弃一部分数据，向后扩张空洞
                    hole.this_end = offset - sizeof(hole);
                } else {
                    // 如果这个空洞太小，而且下一个空洞紧接在后面，那么合并两个空洞
                    read_read_hole(hole.next_beg, hole);
                }
                // 可以正常插入
                back_write(offset - sizeof(hole), sizeof(hole), &hole);
                if (hole.this_end != 0)
                    hole.this_end = offset - hole.this_end;
                if (hole.next_beg != 0)
                    hole.next_beg = offset - hole.next_beg;
                return offset;
            } else {
                // 没有下一个空洞
                return 0;
            }
        }

        void BufferList::read(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            void * dst) const
        {
            assert(offset + size <= data_end_);
            Position p = read_;
            move_front_to(p, offset);
            if (p.buffer + size <= buffer_end()) {
                memcpy(dst, p.buffer, size);
            } else {
                size_t size1 = buffer_end() - p.buffer;
                memcpy(dst, p.buffer, size1);
                memcpy((char *)dst + size1, buffer_beg(), size - size1);
            }
        }

        void BufferList::read(
            boost::uint64_t offset,
            boost::uint32_t size,
            std::deque<boost::asio::const_buffer> & data)
        {
            assert(offset + size <= data_end_);
            Position p = read_;
            move_front_to(p, offset);
            if (p.buffer + size <= buffer_end()) {
                data.push_back(boost::asio::buffer(p.buffer, size));
            } else {// 分段buffer
                size_t size1 = buffer_end() - p.buffer;
                data.push_back(boost::asio::buffer(p.buffer, size1));
                data.push_back(boost::asio::buffer(buffer_beg(), size - size1));
            }
        }

        void BufferList::write(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            void const * src)
        {
            assert(offset + size <= data_end_);
            Position p = read_;
            move_front_to(p, offset);
            if (p.buffer + size <= buffer_end()) {
                memcpy(p.buffer, src, size);
            } else {
                size_t size1 = buffer_end() - p.buffer;
                memcpy(p.buffer, src, size1);
                memcpy(buffer_beg(), (char const *)src + size1, size - size1);
            }
        }

        void BufferList::back_read(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            void * dst) const
        {
            assert(offset + size <= data_end_);
            Position p = read_;
            move_back_to(p, offset);
            if (p.buffer + size <= buffer_end()) {
                memcpy(dst, p.buffer, size);
            } else {
                size_t size1 = buffer_end() - p.buffer;
                memcpy(dst, p.buffer, size1);
                memcpy((char *)dst + size1, buffer_beg(), size - size1);
            }
        }

        void BufferList::back_write(
            boost::uint64_t offset, 
            boost::uint32_t size, 
            void const * src)
        {
            assert(offset + size <= data_end_);
            Position p = read_;
            move_back_to(p, offset);
            if (p.buffer + size <= buffer_end()) {
                memcpy(p.buffer, src, size);
            } else {
                size_t size1 = buffer_end() - p.buffer;
                memcpy(p.buffer, src, size1);
                memcpy(buffer_beg(), (char const *)src + size1, size - size1);
            }
        }

        BufferList::read_buffer_t BufferList::read_buffer(
            boost::uint64_t beg, 
            boost::uint64_t end) const
        {
            boost::asio::const_buffer buffers[2];
            if (end == beg)
                return read_buffer_t();
            char const * buffer = buffer_move_front(read_.buffer, beg - read_.offset);
            if (end - beg < (boost::uint32_t)(buffer_end() - buffer)) {
                buffers[0] = boost::asio::const_buffer(buffer, (size_t)(end - beg));
                return read_buffer_t(buffers, 1);
            } else {
                size_t size = buffer_end() - buffer;
                buffers[0] = boost::asio::const_buffer(buffer, size);
                buffer = buffer_beg();
                beg += size;
                buffers[1] = boost::asio::const_buffer(buffer, (size_t)(end - beg));
                return read_buffer_t(buffers, 2);
            }
        }

        BufferList::write_buffer_t BufferList::write_buffer(
            boost::uint64_t beg, 
            boost::uint64_t end)
        {
            boost::asio::mutable_buffer buffers[2];
            if (end == beg)
                return write_buffer_t();
            char * buffer = buffer_move_front(write_.buffer, beg - write_.offset);
            if (end - beg < (boost::uint32_t)(buffer_end() - buffer)) {
                buffers[0] = boost::asio::mutable_buffer(buffer, (size_t)(end - beg));
                return write_buffer_t(buffers, 1);
            } else {
                size_t size = buffer_end() - buffer;
                buffers[0] = boost::asio::mutable_buffer(buffer, size);
                buffer = buffer_beg();
                beg += size;
                buffers[1] = boost::asio::mutable_buffer(buffer, (size_t)(end - beg));
                return write_buffer_t(buffers, 2);
            }
        }

        // 循环前移
        char * BufferList::buffer_move_front(
            char * buffer, 
            boost::uint64_t offset) const
        {
            buffer += offset;
            if ((long)buffer >= (long)buffer_end()) {
                buffer -= buffer_size_;
            }
            assert((long)buffer >= (long)buffer_beg() && (long)buffer < (long)buffer_end());
            return buffer;
        }

        // 循环后移
        char * BufferList::buffer_move_back(
            char * buffer, 
            boost::uint64_t offset) const
        {
            buffer -= offset;
            if ((long)buffer < (long)buffer_beg()) {
                buffer += buffer_size_;
            }
            assert((long)buffer >= (long)buffer_beg() && (long)buffer < (long)buffer_end());
            return buffer;
        }

        //************************************
        // Method:    move_back 后移
        // FullName:  ppbox::demux::BufferList::move_back
        // Access:    private 
        // Returns:   void
        // Qualifier: const
        // Parameter: Position & position
        // Parameter: boost::uint64_t offset 相对当前位置
        //************************************
        void BufferList::move_back(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_back(position.buffer, offset);
            position.offset -= offset;
        }

        //************************************
        // Method:    move_front 前移
        // FullName:  ppbox::demux::BufferList::move_front
        // Access:    private 
        // Returns:   void
        // Qualifier: const
        // Parameter: Position & position
        // Parameter: boost::uint64_t offset 相对当前位置
        //************************************
        void BufferList::move_front(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_front(position.buffer, offset);
            position.offset += offset;
        }

        //************************************
        // Method:    move_back_to 后移到
        // FullName:  ppbox::demux::BufferList::move_back_to
        // Access:    private 
        // Returns:   void
        // Qualifier: const
        // Parameter: Position & position
        // Parameter: boost::uint64_t offset 文件绝对位置
        //************************************
        void BufferList::move_back_to(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_back(position.buffer, position.offset - offset);
            position.offset = offset;
        }

        //************************************
        // Method:    move_front_to 前移到
        // FullName:  ppbox::demux::BufferList::move_front_to
        // Access:    private 
        // Returns:   void
        // Qualifier: const
        // Parameter: Position & position
        // Parameter: boost::uint64_t offset 文件绝对位置
        //************************************
        void BufferList::move_front_to(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_front(position.buffer, offset - position.offset);
            position.offset = offset;
        }

        //************************************
        // Method:    move_to 移动到
        // FullName:  ppbox::demux::BufferList::move_to
        // Access:    private 
        // Returns:   void
        // Qualifier: const
        // Parameter: Position & position
        // Parameter: boost::uint64_t offset 文件绝对位置
        //************************************
        void BufferList::move_to(
            Position & position, 
            boost::uint64_t offset) const
        {
            if (offset < position.offset) {
                move_back_to(position, offset);
            } else if (position.offset < offset) {
                move_front_to(position, offset);
            }
        }

    } // namespace demux
} // namespace ppbox

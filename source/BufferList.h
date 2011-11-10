// BufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/source/SegmentsBase.h"
#include "ppbox/demux/source/BufferStatistic.h"
#include "ppbox/demux/source/SourceError.h"

#include <util/buffers/Buffers.h>

#include <framework/system/LogicError.h>
#include <framework/container/Array.h>
#include <framework/memory/PrivateMemory.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/timer/TimeCounter.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>

#include <iostream>
#include <fstream>

namespace ppbox
{
    namespace demux
    {

        class BufferList
            : public BufferObserver
            , public BufferStatistic
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferList", 0);

        private:
            struct Hole
            {
                Hole()
                    : this_end(0)
                    , next_beg(0)
                {
                }

                boost::uint64_t this_end;
                boost::uint64_t next_beg;
            };

            struct Position
            {
                Position(
                    boost::uint64_t offset = 0)
                    : offset(offset)
                    , buffer(NULL)
                {
                }

                friend std::ostream & operator << (
                    std::ostream & os, 
                    Position const & p)
                {
                    os << " offset=" << p.offset;
                    os << " buffer=" << (void *)p.buffer;
                    return os;
                }

                boost::uint64_t offset;
                char * buffer;
            };
            /*
                                            offset=500
                                    segment=2   |
            |_____________|_____________|_______|______|_______________|
                         200           400     500    600             800
                                        |              |
                                    seg_beg=400     seg_end=600
            */

            struct PositionEx
                : Position
            {
                PositionEx()
                    : segment((size_t)-1)
                    , seg_beg(0)
                    , seg_end((boost::uint64_t)-1)
                {
                }

                PositionEx(
                    size_t segment, 
                    boost::uint64_t offset)
                    : Position(offset)
                    , segment(segment)
                    , seg_beg(0)
                    , seg_end((boost::uint64_t)-1)
                {
                }

                PositionEx(
                    boost::uint64_t offset)
                    : Position(offset)
                    , segment((size_t)-1)
                    , seg_beg(0)
                    , seg_end((boost::uint64_t)-1)
                {
                }

                friend std::ostream & operator << (
                    std::ostream & os, 
                    PositionEx const & p)
                {
                    os << (Position const &)p;
                    os << " segment=" << p.segment;
                    os << " seg_beg=" << p.seg_beg;
                    os << " seg_end=" << p.seg_end;
                    return os;
                }

                size_t segment;
                boost::uint64_t seg_beg;
                boost::uint64_t seg_end;
            };

        public:
            typedef boost::function<void (
                boost::system::error_code const &,
                size_t)
            > open_response_type;

        public:
            BufferList(
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size, 
                SegmentsBase * segments,
                size_t total_req = 1)
                : segments_(segments)
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
                //TODO:
                //segments_->reserve(seg_num);
                read_.buffer = buffer_beg();
                read_.offset = 0;
                read_.segment = 0;
                read_.seg_beg = 0;
                write_tmp_ = write_ = read_;
                write_tmp_.buffer = NULL;
            }

            ~BufferList()
            {
                if (buffer_)
                    memory_.free_block(buffer_, buffer_size_);
            }

            // 目前只发生在，seek到一个分段，还没有该分段头部数据时，
            // 此时size为head_size_头部数据大小
            boost::system::error_code seek(
                size_t segment, 
                boost::uint64_t offset, 
                boost::uint64_t size, 
                boost::system::error_code & ec)
            {
                PositionEx position(segment, offset);
                if (offset_of_segment(position, ec)) {
                    return ec;
                }
                return seek_to(position.offset, position.offset + size, ec);
            }

            // seek到分段的具体位置offset
            boost::system::error_code seek(
                size_t segment, 
                boost::uint64_t offset, 
                boost::system::error_code & ec)
            {
                PositionEx position(segment, offset);
                if (offset_of_segment(position, ec)) {
                    return ec;
                }
                return seek_to(position.offset, ec);
            }

            void pause(
                boost::uint32_t time)
            {
                expire_pause_time_ = Time::now() + Duration::milliseconds(time);
            }

            void set_time_out(
                boost::uint32_t time_out)
            {
                time_out_ = time_out / 1000;
            }

            void set_max_try(
                size_t max_try)
            {
                max_try_ = max_try;
            }

            boost::system::error_code cancel(
                boost::system::error_code & ec)
            {
                source_error_ = boost::asio::error::operation_aborted;
                return segments_->segment_cancel(write_.segment, ec);
            }

            boost::system::error_code close(
                boost::system::error_code & ec)
            {
                return close_all_segment(ec);
            }

            boost::system::error_code prepare(
                boost::uint32_t amount, 
                boost::system::error_code & ec)
            {
                ec = source_error_;
                while (1) {
                    if (ec) {
                    } else if (write_.offset >= write_hole_.this_end) {
                        ec = boost::asio::error::eof;
                    } else if (write_.offset >= read_.offset + buffer_size_) {
                        ec = boost::asio::error::no_buffer_space;
                        break;
                    } else if (source_closed_ && open_segment(true, ec)) {
                    } else if (segments_->segment_is_open(ec)) {
                        // 请求的分段打开成功，更新 (*segments_) 信息
                        update_segments(ec);

                        framework::timer::TimeCounter tc;
                        size_t bytes_transferred = segments_->segment_read(
                            write_buffer(amount),
                            ec
                            );
                        if (tc.elapse() > 10) {
                            LOG_S(framework::logger::Logger::kLevelDebug, 
                                "[prepare] read elapse: " << tc.elapse() 
                                << " bytes_transferred: " << bytes_transferred);
                        }
                        increase_download_byte(bytes_transferred);
                        move_front(write_, bytes_transferred);
                        if (ec && !segments_->continuable(ec)) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "[prepare] read_some: " << ec.message() << 
                                " --- failed " << (*segments_)[write_.segment].num_try << " times");
                            if (ec == boost::asio::error::eof) {
                                LOG_S(framework::logger::Logger::kLevelDebug, 
                                    "[prepare] read eof, write_.offset: " << write_.offset
                                    << " write_hole_.this_end: " << write_hole_.this_end);
                            }
                        }
                        if (data_end_ < write_.offset)
                            data_end_ = write_.offset;
                    } else {
                        if (!segments_->continuable(ec)) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "[prepare] open_segment: " << ec.message() << 
                                " --- failed " << (*segments_)[write_.segment].num_try << " times");
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

                    bool is_error = on_error(ec);
                    if (is_error) {
                        if (ec == boost::asio::error::eof) {
                            open_segment(true, ec);
                            if (ec && !on_error(ec))
                                break;
                        } else {
                            open_segment(false, ec);
                        }
                    } else {
                        break;
                    }
                }
                return ec;
            }

            void async_prepare(
                boost::uint32_t amount, 
                open_response_type const & resp)
            {
                amount_ = amount;
                resp_ = resp;
                handle_async(boost::system::error_code(), 0);
            }

            void handle_async(
                boost::system::error_code const & ecc, 
                size_t bytes_transferred)
            {
                boost::system::error_code ec = ecc;
                if (ec && !segments_->continuable(ec) && bytes_transferred > 0) {
                    LOG_S(framework::logger::Logger::kLevelAlarm, 
                        "[handle_async] read_some: " << ec.message() << 
                        " --- failed " << (*segments_)[write_.segment].num_try << " times");
                    if (ec == boost::asio::error::eof) {
                        LOG_S(framework::logger::Logger::kLevelDebug, 
                            "[handle_async] read eof, write_.offset: " << write_.offset
                            << " write_hole_.this_end: " << write_hole_.this_end);
                    }
                }
                if (bytes_transferred > 0) {
                    increase_download_byte(bytes_transferred);
                    move_front(write_, bytes_transferred);
                    if (data_end_ < write_.offset)
                        data_end_ = write_.offset;
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
                    bool is_error = on_error(ec);
                    if (is_error) {
                        if (ec == boost::asio::error::eof) {
                            reset_zero_interval();
                            time_block_ = 0;
                            async_open_segment(true, boost::bind(&BufferList::handle_async, this, _1, 0));
                            return;
                        } else {
                            async_open_segment(false, boost::bind(&BufferList::handle_async, this, _1, 0));
                            return;
                        }
                    } else {
                        close_segment(ec);
                    }
                } else if (write_.offset >= write_hole_.this_end) {
                    ec = boost::asio::error::eof;
                    return handle_async(ec, 0);
                } else if (write_.offset >= read_.offset + buffer_size_) {
                    ec = boost::asio::error::no_buffer_space;
                } else if (source_closed_) {
                    async_open_segment(true, boost::bind(&BufferList::handle_async, this, _1, 0));
                    return;
                } else {
                    update_segments(ec);
                    segments_->segment_async_read(
                        write_buffer(amount_),
                        boost::bind(&BufferList::handle_async, this, _1, _2));
                    return;
                }
                response(ec);
            }

            void response(
                boost::system::error_code const & ec)
            {
                write_tmp_ = write_;
                write_tmp_.buffer = NULL;
                write_hole_tmp_ = write_hole_;
                //boost::system::error_code ecc = boost::system::error_code();
                //send_request(true, ecc);
                open_response_type resp;
                resp.swap(resp_);
                resp(ec, 0);
            }

            boost::system::error_code prepare_at_least(
                boost::uint32_t amount, 
                boost::system::error_code & ec)
            {
                return prepare(amount < prepare_size_ ? prepare_size_ : amount, ec);
            }

            void async_prepare_at_least(
                boost::uint32_t amount, 
                open_response_type const & resp)
            {
                async_prepare(amount < prepare_size_ ? prepare_size_ : amount, resp);
            }

            boost::system::error_code prepare_all(
                boost::system::error_code & ec)
            {
                if (write_.segment >= segments_->total_segments()) {
                    return ec = boost::asio::error::eof;
                }
                Segment & segment = (*segments_)[write_.segment];
                if (segment.total_state != Segment::is_valid) {
                    do {
                        prepare(prepare_size_, ec);
                    } while (!ec && write_.seg_end > write_.offset);
                    if (ec == boost::asio::error::eof && segment.total_state == Segment::by_guess)
                        ec = boost::system::error_code();
                } else {
                    prepare((boost::uint32_t)(write_.seg_end - write_.offset), ec);
                }
                if (!ec) {
                    close_all_segment(ec);
                    offset_to_segment(write_, ec);
                    if (ec == boost::asio::error::eof)
                        ec = boost::system::error_code();
                }
                return ec;
            }

            boost::system::error_code peek(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec)
            {
                offset += read_.seg_beg;
                assert(offset >= read_.offset && offset + size <= read_.seg_end);
                if (offset < read_.offset) {
                    ec = framework::system::logic_error::out_of_range;
                } else if (offset + size > read_.seg_end) {
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
                return ec;
            }

            boost::system::error_code peek(
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec)
            {
                return peek(read_.offset - read_.seg_beg, size, data, ec);
            }

            boost::system::error_code peek(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec)
            {
                offset += read_.seg_beg;
                assert(offset >= read_.offset && offset + size <= read_.seg_end);
                if (offset < read_.offset) {
                    ec = framework::system::logic_error::out_of_range;
                } else if (offset + size > read_.seg_end) {
                    ec = boost::asio::error::eof;
                } else {
                    if (offset + size <= write_.offset) {
                        prepare_at_least(0, ec);
                    } else {
                        prepare_at_least((boost::uint32_t)(offset + size - write_.offset), ec);
                    }
                    if (offset + size <= write_.offset) {
                        read(offset, size, data);
                        ec = boost::system::error_code();
                    }
                }
                return ec;
            }

            boost::system::error_code peek(
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec)
            {
                return peek(read_.offset - read_.seg_beg, size, data, ec);
            }

            boost::system::error_code read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec)
            {
                peek(offset, size, data, ec);
                offset += read_.seg_beg;
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

            boost::system::error_code read(
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec)
            {
                return read(read_.offset - read_.seg_beg, size, data, ec);
            }

            boost::system::error_code drop(
                boost::uint32_t size, 
                boost::system::error_code & ec)
            {
                return read_seek_to(read_.offset + size, ec);
            }

            boost::system::error_code drop_to(
                boost::uint64_t offset, 
                boost::system::error_code & ec)
            {
                if (read_.seg_beg + offset < read_.offset) {
                    return ec = framework::system::logic_error::invalid_argument;
                } else {
                    return read_seek_to(read_.seg_beg + offset, ec);
                }
            }

            /**
                drop_all 
                丢弃当前分段的所有剩余数据，并且更新当前分段信息
             */
            boost::system::error_code drop_all(
                boost::system::error_code & ec)
            {
                if ((*segments_)[read_.segment].total_state < Segment::is_valid) {
                    assert(read_.segment == write_.segment);
                    write_.seg_end = write_.offset;
                    read_.seg_end = write_.offset;
                    (*segments_)[read_.segment].total_state = Segment::by_guess;
                    (*segments_)[read_.segment].file_length = read_.seg_end - read_.seg_beg;
                    LOG_S(framework::logger::Logger::kLevelInfor, "[drop_all] guess segment size " << (*segments_)[read_.segment].file_length);
                }
                if (read_.seg_end == (boost::uint64_t)-1) {
                    size_t seg = read_.segment ;
                    offset_to_segment(read_, ec);
                    if (read_.segment == seg + 1)
                        return ec;
                }
                read_seek_to(read_.seg_end, ec) || 
                    offset_to_segment(read_, ec);
                return ec;
            }

            // 当前所有分段是否下载完成
            bool write_at_end()
            {
                return write_.offset == write_.seg_end && (write_.segment + 1) == segments_->total_segments();
            }

            void clear()
            {
                //segments_->clear();
                read_.buffer = buffer_beg();
                read_.offset = 0;
                read_.segment = 0;
                read_.seg_beg = 0;
                write_ = read_;
                read_hole_.this_end = 0;
                read_hole_.next_beg = 0;
                write_hole_.this_end = 0;
                write_hole_.next_beg = 0;
                time_block_ = 0;
                time_out_ = 0;
                source_closed_ = true;
                data_beg_ = 0;
                data_end_ = 0;
                seek_end_ = (boost::uint64_t)-1;
                amount_ = 0;
                expire_pause_time_ = Time::now();
                clear_error();
            }

            bool is_ignore()
            {
                return !source_error_;
            }

        public:
            boost::uint32_t read_avail() const
            {
                if (read_.seg_end > write_.offset) {
                    return (boost::uint32_t)(write_.offset - read_.offset);
                } else {
                    return (boost::uint32_t)(read_.seg_end - read_.offset); // file_length
                }
            }

            boost::uint32_t segment_read_avail(
                size_t segment) const
            {
                if (read_.segment == segment) {
                    return read_avail();
                } else if (segment < read_.segment || segment >= segments_->total_segments()) {
                    return (boost::uint32_t)0;
                } else {
                    PositionEx position(segment, 0);
                    boost::system::error_code ec;
                    if (offset_of_segment(position, ec) || position.offset > data_end_) {
                        return (boost::uint32_t)0;
                    } else {
                        Hole hole = write_hole_;
                        boost::uint64_t beg = read_.offset;
                        while (hole.next_beg < position.offset) {
                            beg = write_hole_.this_end;
                            read(hole.next_beg, sizeof(hole), &hole);
                        }
                        boost::uint64_t end = data_end_;
                        if (end > hole.next_beg)
                            end = hole.next_beg;
                        if (beg >= position.offset) {
                            return (boost::uint32_t)0;
                        } else if ((*segments_)[segment].total_state != Segment::is_valid) {
                            return (boost::uint32_t)0;
                        } else if (position.offset + (*segments_)[segment].file_length < end) {
                            return (boost::uint32_t)(*segments_)[segment].file_length;
                        } else {
                            return (boost::uint32_t)(end - position.offset);
                        }
                    }
                }
            }

            boost::uint64_t read_back() const
            {
                if (read_.seg_end > write_.offset) {
                    return (boost::uint64_t)(write_.offset - read_.seg_beg);
                } else {
                    return (boost::uint64_t)(read_.seg_end - read_.seg_beg); // file_length
                }
            }

            boost::uint64_t segment_read_back(
                size_t segment) const
            {
                if (read_.segment == segment) {
                    return read_back();
                } else {
                    return segment_read_avail(segment);
                }
            }

            boost::uint64_t read_front() const
            {
                return (boost::uint64_t)(read_.offset - read_.seg_beg);
            }

            boost::uint64_t segment_read_front(
                size_t segment) const
            {
                if (read_.segment == segment) {
                    return read_front();
                } else if (segment < segments_->total_segments()) {
                    return (*segments_)[segment].begin;
                } else {
                    return boost::uint64_t(0);
                }
            }

            boost::uint64_t segment_size(
                size_t segment) const
            {
                if (segment < segments_->total_segments()) {
                    return (*segments_)[segment].file_length;
                } else {
                    return boost::uint64_t(-1);
                }
            }

            boost::uint32_t write_avail() const
            {
                return (boost::uint32_t)buffer_size_ - read_avail();
            }

            size_t read_segment() const
            {
                return read_.segment;
            }

            size_t write_segment() const
            {
                return write_.segment;
            }

            size_t read_offset() const
            {
                return read_.offset;
            }

            size_t write_offset() const
            {
                return write_.offset;
            }

        public:
            class TerminateBuffer
            {
            public:
                ~TerminateBuffer()
                {
                    assert(tmp_ == -1);
                }

                char * data() const
                {
                    return data_;
                }

                size_t size() const
                {
                    return size_;
                }

            private:
                friend class BufferList;

                TerminateBuffer(
                    char * data, 
                    size_t size, 
                    char * beg, 
                    char * end)
                    : size_(size)
                {
                    if (data + size >= end) {
                        data_ = new char[size + 1];
                        size = end - data;
                        memcpy(data_, data, size);
                        memcpy(data_ + size, beg, size_ - size);
                        data_[size_] = '\0';
                        tmp_ = -1;
                    } else {
                        data_ = data;
                        tmp_ = data[size];
                        data[size] = '\0';
                    }
                }

                void clear()
                {
                    if (tmp_ == -1) {
                        delete [] data_;
                    } else {
                        data_[size_] = (char)tmp_;
                        tmp_ = -1;
                    }
                    data_ = NULL;
                }

            private:
                char * data_;
                size_t size_;
                int tmp_;
            };

            TerminateBuffer get_terminate_buffer()
            {
                boost::uint64_t end = write_.offset;
                if (end > read_.seg_end)
                    end = read_.seg_end;
                return TerminateBuffer(read_.buffer, (size_t)(end - read_.offset), buffer_beg(), buffer_end());
            }

            void free_terminate_buffer(
                TerminateBuffer & buf)
            {
                buf.clear();
            }

            typedef util::buffers::Buffers<
                boost::asio::const_buffer, 2
            > read_buffer_t;

            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

            read_buffer_t read_buffer() const
            {
                boost::uint64_t beg = read_.offset;
                boost::uint64_t end = write_.offset;
                if (end > read_.seg_end)
                    end = read_.seg_end;
                return read_buffer(beg, end);
            }

            write_buffer_t write_buffer()
            {
                boost::uint64_t beg = write_.offset;
                boost::uint64_t end = read_.offset + buffer_size_;
                if (end > write_hole_.this_end)
                    end = write_hole_.this_end;
                return write_buffer(beg, end);
            }

            write_buffer_t write_buffer(
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

            read_buffer_t segment_read_buffer(
                size_t segment) const
            {
                boost::uint64_t beg = 0;
                boost::uint64_t end = 0;
                if (segment == read_.segment) {
                    return read_buffer();
                } else if (segment == write_.segment) {
                    beg = write_.seg_beg;
                    end = write_.offset;
                }
                return read_buffer(beg, end);
            }

            /*read_buffer_t segment_read_buffer(
                size_t segment) const
            {
                boost::uint64_t beg = segment_read_front(segment);
                boost::uint64_t end = segment_read_back(segment);
                boost::system::error_code ec;
                PositionEx position_beg(segment, beg);
                PositionEx position_end(segment, end);
                offset_of_segment(position_beg, ec);
                offset_of_segment(position_end, ec);
                return read_buffer(position_beg.offset, position_end.offset);
            }*/

            void add_request(
                boost::system::error_code & ec)
            {
                if (sended_req_ && resp_.empty()) {
                    send_request(true, ec);
                }
            }

            boost::uint32_t get_segment_num_try(
                boost::system::error_code & ec) const
            {
                return (*segments_)[write_.segment].num_try;
            }

            void set_total_req(
                size_t num)
            {
                total_req_ = num;
                if (segments_->total_segments()> 0) {
                    boost::system::error_code ec;
                    send_request(true, ec);
                }
            }

            void decrease_total_req()
            {
                if (total_req_ > 1) {
                    total_req_--;
                    if (segments_->total_segments()> 0) {
                        boost::system::error_code ec;
                        send_request(true, ec);
                    }
                }
            }

            void increase_req()
            {
                sended_req_++;
                source_closed_ = false;
            }

        private:
            // 返回false表示不能再继续了
            bool on_error(
                boost::system::error_code& ec)
            {
                if (segments_->continuable(ec)) {
                    time_block_ = get_zero_interval();
                    if (time_out_ > 0 && time_block_ > time_out_) {
                        LOG_S(framework::logger::Logger::kLevelAlarm,
                            "source.read_some: timeout" << 
                            " --- failed " << (*segments_)[write_.segment].num_try << " times");
                        ec = boost::asio::error::timed_out;
                        if (can_retry()) {
                            return true;
                        }
                    }  else {
                        return false;
                    }
                } else if (ec == boost::asio::error::eof) {
                    if (write_.offset >= write_hole_.this_end) {
                        return true;
                    } else if ((*segments_)[write_.segment].total_state == Segment::not_exist) {
                        (*segments_)[write_.segment].total_state = Segment::by_guess;
                        write_.seg_end = write_.offset;
                        write_hole_.this_end = write_.offset;
                        if (read_.segment == write_.segment)
                            read_.seg_end = write_.seg_end;
                        (*segments_)[write_.segment].file_length = write_.seg_end - write_.seg_beg;
                        LOG_S(framework::logger::Logger::kLevelInfor, 
                            "[on_error] guess segment size " << (*segments_)[write_.segment].file_length);
                        return true;
                    } else if (can_retry()) {
                        ec = boost::asio::error::connection_aborted;
                        return true;
                    }
                } else if(segments_->recoverable(ec)) {
                    if (can_retry()) {
                        return true;
                    }
                }
                segments_->on_error(ec);
                return !ec;
            }

            bool can_retry() const
            {
                return (*segments_)[write_.segment].num_try < max_try_;
            }

            boost::system::error_code seek_to(
                boost::uint64_t offset, 
                boost::system::error_code & ec)
            {
                return seek_to(offset, boost::uint64_t(-1), ec);
            }

            boost::system::error_code seek_to(
                boost::uint64_t offset, 
                boost::uint64_t end, 
                boost::system::error_code & ec)
            {
                boost::uint64_t write_offset = write_.offset;
                if (data_end_ > data_beg_ + buffer_size_)
                    data_beg_ = data_end_ - buffer_size_;
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "seek_to " << offset);
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
                            LOG_S(framework::logger::Logger::kLevelDebug2, 
                                "backward data: " << data_beg_ << "-" << data_end_);
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
                            LOG_S(framework::logger::Logger::kLevelDebug2, "advance data: " << data_beg_ << "-" << data_end_);
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
                            LOG_S(framework::logger::Logger::kLevelDebug2, 
                                "advance data: " << data_beg_ << "-" << data_end_);
                        }
                        move_front_to(write_, offset);
                        read_ = write_;
                        read_hole_next_beg = offset;
                    }
                    // lay a read hole
                    read_hole_.next_beg = write_read_hole(read_hole_next_beg, read_hole_);
                }
                offset_to_segment(write_, ec);
                offset_to_segment(read_, ec);
                if (write_.offset != write_offset) {
                    // close source for open from new offset
                    boost::system::error_code ec1;
                    close_all_segment(ec1);
                }
                if (!ec) {
                    seek_end_ = end;
                }
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "after seek_to " << offset);
                dump();
                return ec;
            }

            /**
                只在当前分段移动read指针，不改变write指针
                只能在当前分段内移动
                即使移动到当前段的末尾，也不可以改变read内的当前分段
             */
            boost::system::error_code read_seek_to(
                boost::uint64_t offset, 
                boost::system::error_code & ec)
            {
                assert(offset >= read_.offset && offset <= read_.seg_end);
                if (offset < read_.offset) {
                    ec = framework::system::logic_error::out_of_range;
                } else if (offset > read_.seg_end) {
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

            boost::system::error_code next_write_hole(
                PositionEx & pos, 
                Hole & hole, 
                boost::system::error_code & ec)
            {
                boost::uint64_t next_offset = read_write_hole(hole.next_beg, hole);
                if (pos.buffer != NULL) {
                    move_front_to(pos, next_offset);
                } else {
                    pos.offset = next_offset;
                }

                if (offset_to_segment(pos, ec)) {
                    hole.this_end = hole.next_beg = pos.offset;
                    return ec;
                }
                if (pos.segment >= segments_->total_segments()|| pos.offset >= seek_end_) {
                    hole.this_end = hole.next_beg = pos.offset;
                    return ec = source_error::no_more_segment;
                }

                // W     e^b    e----b      e---
                // 如果当这个分段不能完全填充当前空洞，会切分出一个小空洞，需要插入
                boost::uint64_t end = pos.seg_end;
                if (end > seek_end_) {   // 一般这种可能性是先下头部数据的需求
                    end = seek_end_;
                }
                if (end < hole.this_end) {
                    if (write_write_hole(end, hole) == boost::uint64_t(-1))
                        data_end_ = pos.offset;
                    hole.this_end = end;
                    hole.next_beg = end;
                }

                return ec;
            }

            boost::system::error_code open_segment(
                bool is_next_segment, 
                boost::system::error_code & ec)
            {
                if (is_next_segment) {
                    reset_zero_interval();
                    time_block_ = 0;
                    close_segment(ec);
                    (*segments_)[write_.segment].num_try = 0;
                } else {
                    reset_zero_interval();
                    close_all_segment(ec);
                }

                if (Time::now() <= expire_pause_time_) {
                    ec = boost::asio::error::would_block;
                    return ec;
                }

                send_request(is_next_segment, ec);

                if (ec && !segments_->continuable(ec)) {
                    if (!ec)
                        ec = boost::asio::error::would_block;
                    return ec;
                }

                if (is_next_segment) {
                    if (next_write_hole(write_, write_hole_, ec)) {
                        assert(0);
                        return ec;
                    }
                }

                LOG_S(framework::logger::Logger::kLevelAlarm, 
                    "[open_segment] write_.segment: " << write_.segment << 
                    " write_.offset: " << write_.offset - write_.seg_beg << 
                    " write_hole_.this_end: " << write_hole_.this_end - write_.seg_beg);

                segments_->on_seg_beg(write_.segment);
                source_closed_ = false;

                return ec;
            }

            void async_open_segment(
                bool is_next_segment, 
                open_response_type const & resp)
            {
                boost::system::error_code ec;

                if (is_next_segment) {
                    close_segment(ec);
                    if (next_write_hole(write_, write_hole_, ec)) {
                        resp(ec, 0);
                        return;
                    }
                    (*segments_)[write_.segment].num_try = 0;
                } else {
                    reset_zero_interval();
                    close_segment(ec);
                }

                source_closed_ = false;
                sended_req_++;
                ++(*segments_)[write_.segment].num_try;

                segments_->on_seg_beg(write_.segment);
                segments_->segment_async_open(
                    write_.segment, 
                    write_.offset - write_.seg_beg, 
                    write_hole_.this_end == boost::uint64_t(-1) || write_hole_.this_end == write_.seg_end ? 
                    boost::uint64_t(-1) : write_hole_.this_end - write_.seg_beg, 
                    boost::bind(resp, _1, 0));
            }

            boost::system::error_code close_segment(
                boost::system::error_code & ec)
            {
                if (sended_req_) {
                    segments_->segment_close(write_.segment, ec);
                    sended_req_--;
                } else {
                    return ec;
                }

                if (!source_closed_) {
                    LOG_S(framework::logger::Logger::kLevelAlarm, 
                        "[close_segment] write_.segment: " << write_.segment << 
                        " write_.offset: " << write_.offset << 
                        " write_hole_.this_end: " << write_hole_.this_end);
                    source_closed_ = true;
                }
                return ec;
            }

            boost::system::error_code close_all_segment(
                boost::system::error_code & ec)
            {
                write_tmp_ = write_;
                write_tmp_.buffer = NULL;
                write_hole_tmp_ = write_hole_;
                if (!sended_req_) {
                    return ec = boost::system::error_code();
                }

                for (size_t i = 0; i < sended_req_; ++i) {
                    segments_->segment_close(write_.segment, ec);
                }
                sended_req_ = 0;

                if (!source_closed_) {
                    LOG_S(framework::logger::Logger::kLevelAlarm, 
                        "[close_segment] write_.segment: " << write_.segment << 
                        " write_.offset: " << write_.offset << 
                        " write_hole_.this_end: " << write_hole_.this_end);
                    segments_->on_seg_end(write_.segment);
                    source_closed_ = true;
                }

                return ec;
            }

            void dump()
            {
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "buffer:" << (void *)buffer_beg() << "-" << (void *)buffer_end());
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "data:" << data_beg_ << "-" << data_end_);
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "read:" << read_);
                LOG_S(framework::logger::Logger::kLevelDebug2, 
                    "write:" << write_);
                boost::uint64_t offset = read_hole_.next_beg;
                Hole hole;
                offset = read_read_hole(offset, hole);
                while (1) {
                    LOG_S(framework::logger::Logger::kLevelDebug2, 
                        "read_hole:" << offset << "-" << hole.this_end);
                    if (hole.this_end == 0)
                        break;
                    offset = read_read_hole(hole.next_beg, hole);
                }
                hole = write_hole_;
                offset = write_.offset;
                while (1) {
                    LOG_S(framework::logger::Logger::kLevelDebug2, 
                        "write_hole:" << offset << "-" << hole.this_end);
                    if (hole.next_beg == boost::uint64_t(-1))
                        break;
                    offset = read_write_hole(hole.next_beg, hole);
                }
            }

            boost::uint64_t read_write_hole(
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
                    return offset;
                }
            }

            boost::uint64_t write_write_hole(
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
                    // 可以正常插入
                    write(offset, sizeof(hole), &hole);
                    return offset;
                } else {
                    // 没有下一个空洞
                    return boost::uint64_t(-1);
                }
            }

            boost::uint64_t read_read_hole(
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
                    return offset;
                }
            }

            boost::uint64_t write_read_hole(
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
                    return offset;
                } else {
                    // 没有下一个空洞
                    return 0;
                }
            }

            void read(
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

            void read(
                boost::uint64_t offset,
                boost::uint32_t size,
                std::deque<boost::asio::const_buffer> & data)
            {
                assert(offset + size <= data_end_);
                Position p = read_;
                move_front_to(p, offset);
                if (p.buffer + size <= buffer_end()) {
                    data.push_back(boost::asio::buffer(p.buffer, size));
                } else {
                    size_t size1 = buffer_end() - p.buffer;
                    data.push_back(boost::asio::buffer(p.buffer, size1));
                    data.push_back(boost::asio::buffer(buffer_beg(), size - size1));
                }
            }

            void write(
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

            void back_read(
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

            void back_write(
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

            read_buffer_t read_buffer(
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

            write_buffer_t write_buffer(
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

            boost::system::error_code offset_of_segment(
                PositionEx & position, 
                boost::system::error_code & ec) const
            {
                boost::uint64_t offset = position.offset;
                assert((position.segment < segments_->total_segments()&& offset <= (*segments_)[position.segment].file_length)
                     || (position.segment == segments_->total_segments()&& offset == 0));
                if ((position.segment >= segments_->total_segments()|| offset > (*segments_)[position.segment].file_length)
                    && (position.segment != segments_->total_segments()|| offset != 0))
                        return ec = framework::system::logic_error::out_of_range;
                for (size_t i = 0; i < position.segment; ++i) {
                    if ((*segments_)[i].total_state < Segment::is_valid)
                        return ec = framework::system::logic_error::out_of_range;
                    offset += (*segments_)[i].file_length;
                }
                position.seg_beg = offset - position.offset;
                position.seg_end = 
                    (position.segment < segments_->total_segments()&& 
                    (*segments_)[position.segment].total_state >= Segment::is_valid) ? 
                    position.seg_beg + (*segments_)[position.segment].file_length : boost::uint64_t(-1);
                position.offset = offset;
                return ec = boost::system::error_code();
            }

            boost::system::error_code offset_to_segment(
                PositionEx & position, 
                boost::system::error_code & ec) const
            {
                boost::uint64_t offset = position.offset;
                size_t segment = 0;
                for (segment = 0; segment < segments_->total_segments()
                    && (*segments_)[segment].total_state >= Segment::is_valid 
                    && (*segments_)[segment].file_length <= offset; ++segment)
                    offset -= (*segments_)[segment].file_length;
                // 增加offset==0，使得position.offset为所有分段总长时，也认为是有效的
                assert(segment < segments_->total_segments()|| offset == 0);
                if (segment < segments_->total_segments()|| offset == 0) {
                    position.segment = segment;
                    position.seg_beg = position.offset - offset;
                    position.seg_end = 
                        (segment < segments_->total_segments()&& 
                        (*segments_)[position.segment].total_state >= Segment::is_valid) ? 
                            position.seg_beg + (*segments_)[position.segment].file_length : boost::uint64_t(-1);
                    return ec = boost::system::error_code();
                } else {
                    return ec = framework::system::logic_error::out_of_range;
                }
            }

            char const * buffer_beg() const
            {
                return buffer_;
            }

            char * buffer_beg()
            {
                return buffer_;
            }

            char const * buffer_end() const
            {
                return buffer_ + buffer_size_;
            }

            char * buffer_end()
            {
                return buffer_ + buffer_size_;
            }

            char * buffer_move_front(
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

            char * buffer_move_back(
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

            void move_back(
                Position & position, 
                boost::uint64_t offset) const
            {
                position.buffer = buffer_move_back(position.buffer, offset);
                position.offset -= offset;
            }

            void move_front(
                Position & position, 
                boost::uint64_t offset) const
            {
                position.buffer = buffer_move_front(position.buffer, offset);
                position.offset += offset;
            }

            void move_back_to(
                Position & position, 
                boost::uint64_t offset) const
            {
                position.buffer = buffer_move_back(position.buffer, position.offset - offset);
                position.offset = offset;
            }

            void move_front_to(
                Position & position, 
                boost::uint64_t offset) const
            {
                position.buffer = buffer_move_front(position.buffer, offset - position.offset);
                position.offset = offset;
            }

            void move_to(
                Position & position, 
                boost::uint64_t offset) const
            {
                if (offset < position.offset) {
                    move_back_to(position, offset);
                } else if (position.offset < offset) {
                    move_front_to(position, offset);
                }
            }

            void clear_error()
            {
                source_error_ = boost::system::error_code();
            }

            void update_segments(
                boost::system::error_code & ec)
            {
                if ((*segments_)[write_.segment].total_state == Segment::not_init) {
                    (*segments_)[write_.segment].file_length = segments_->total(ec);
                    if (ec) {
                        (*segments_)[write_.segment].file_length = boost::uint64_t(-1);
                        (*segments_)[write_.segment].total_state = Segment::not_exist;
                    } else {
                        (*segments_)[write_.segment].total_state = Segment::is_valid;
                        write_.seg_end = write_.seg_beg + (*segments_)[write_.segment].file_length;
                        if (write_hole_.this_end >= write_.seg_end)
                            write_hole_.this_end = write_.seg_end;
                        if (read_.segment == write_.segment)
                            read_.seg_end = write_.seg_end;
                    }
                }
            }

            boost::system::error_code send_request(
                bool is_next_segment,
                boost::system::error_code & ec)
            {
                for (size_t i = 0; i < total_req_ - sended_req_; ) {
                    PositionEx write_tmp = write_tmp_;
                    Hole write_hole_tmp = write_hole_tmp_;
                    if (is_next_segment) {
                        boost::uint64_t data_end_tmp = data_end_;
                        if (data_end_ < write_hole_tmp_.this_end 
                            && write_hole_tmp_.this_end <= write_tmp_.seg_end 
                            && write_hole_tmp_.this_end != boost::uint64_t(-1)) {
                            data_end_ = write_hole_tmp_.this_end;
                        }
                        if (next_write_hole(write_tmp_, write_hole_tmp_, ec)) {
                            if (sended_req_ && ec == source_error::no_more_segment) {
                                ec.clear();
                            }
                            data_end_ = data_end_tmp;
                            write_tmp_ = write_tmp;
                            write_hole_tmp_ = write_hole_tmp;
                            break;
                        }
                        data_end_ = data_end_tmp;
                    }
                    ++(*segments_)[write_tmp_.segment].num_try;
                    segments_->segment_open(write_tmp_.segment, write_tmp_.offset - write_tmp_.seg_beg, 
                        write_hole_tmp_.this_end == boost::uint64_t(-1) || write_hole_tmp_.this_end == write_tmp_.seg_end ? 
                        boost::uint64_t(-1) : write_hole_tmp_.this_end - write_tmp_.seg_beg, ec);
                    if (ec) {
                        if (segments_->continuable(ec)) {
                            if (sended_req_) // 如果已经发过一个请求
                                ec.clear();
                        } else {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "[open_segment] segments_->open_segment: " << ec.message() << 
                                " --- failed " << (*segments_)[write_tmp_.segment].num_try << " times");
                            write_tmp_ = write_tmp;
                            write_hole_tmp_ = write_hole_tmp;
                            break;
                        }
                    }
                    is_next_segment = true;
                    sended_req_++;
                }
                return ec;
            }

        private:
            SegmentsBase * segments_;
            size_t max_try_;    // 尝试重连最大次数，决定不断点续传的标志
            char * buffer_;
            boost::uint32_t buffer_size_;   // buffer_ 分配的大小
            boost::uint32_t prepare_size_;  // 下载一次，最大的读取数据大小
            boost::uint32_t time_block_;    // 阻塞次数（秒）
            boost::uint32_t time_out_;      // 超时时间（秒）
            bool source_closed_;            // ture，可以调用 open_segment(), 调用 segments_->open_segment()如果失败，为false
            boost::system::error_code source_error_;    // 下载的错误码
            boost::uint64_t data_beg_;
            boost::uint64_t data_end_;
            boost::uint64_t seek_end_;  // 一般在seek操作时，如果获取头部数据，值为当前分段之前的分段总长+当前分段的head_size_；否则为-1
            PositionEx read_;
            Hole read_hole_;
            PositionEx write_;
            Hole write_hole_;

            PositionEx write_tmp_;
            Hole write_hole_tmp_;

            framework::memory::PrivateMemory memory_;

            boost::uint32_t amount_;    // 用于异步下载时，读取一次最大的数据大小
            open_response_type resp_;
            Time expire_pause_time_;

            size_t total_req_;
            size_t sended_req_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

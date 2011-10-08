// BufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/source/BufferStatistic.h"

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

        template <
            typename Source
        >
        class BufferList
            : public BufferObserver
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

            struct Segment
            {
                Segment(
                    boost::uint64_t total, 
                    size_t max_try)
                    : begin(0)
                    , total(total)
                    , total_state(is_valid)
                    , num_try(0)
                    , max_try(max_try)
                {
                }

                Segment(
                    size_t max_try = size_t(-1))
                    : begin(0)
                    , total((boost::uint64_t)-1)
                    , total_state(not_init)
                    , num_try(0)
                    , max_try(max_try)
                {
                }

                enum TotalStateEnum
                {
                    not_init, 
                    not_exist, 
                    is_valid, 
                    by_guess, 
                };

                bool can_retry() const
                {
                    return num_try < max_try;
                }

                boost::uint64_t begin;
                boost::uint64_t total;
                TotalStateEnum total_state;
                size_t num_try;
                size_t max_try;
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
                size_t seg_num = 1)
                : max_try_(size_t(-1))
                , buffer_(NULL)
                , buffer_size_(framework::memory::MemoryPage::align_page(buffer_size))
                , prepare_size_(prepare_size)
                , time_out_(0)
                , source_closed_(true)
                , data_beg_(0)
                , data_end_(0)
                , seek_end_(boost::uint64_t(-1))
            {
                buffer_ = (char *)memory_.alloc_block(buffer_size_);
                segments_.reserve(seg_num);
                read_.buffer = buffer_beg();
                read_.offset = 0;
                read_.segment = 0;
                read_.seg_beg = 0;
                write_ = read_;
            }

            ~BufferList()
            {
                if (buffer_)
                    memory_.free_block(buffer_, buffer_size_);
            }

            size_t add_segment(
                boost::system::error_code & ec)
            {
                size_t index = segments_.size();
                segments_.push_back(Segment(max_try_));
                return index;
            }

            size_t add_segment(
                boost::uint64_t total, 
                boost::system::error_code & ec)
            {
                size_t index = segments_.size();
                segments_.push_back(Segment(total, max_try_));
                if (index == read_.segment)
                    offset_to_segment(read_, ec);
                return index;
            }

            size_t add_segments(
                size_t num, 
                boost::system::error_code & ec)
            {
                size_t index = segments_.size();
                segments_.insert(segments_.end(), num, Segment(max_try_));
                return index;
            }

            size_t add_segments(
                std::vector<boost::uint64_t> const & segment_sizes, 
                boost::system::error_code & ec)
            {
                size_t index = segments_.size();
                for (size_t i = 0; i < segment_sizes.size(); ++i) {
                    segments_.push_back(Segment(segment_sizes[i], max_try_));
                }
                if (index == read_.segment)
                    offset_to_segment(read_, ec);
                return index;
            }

            void set_segment_begin(
                size_t segment, 
                boost::uint64_t begin, 
                boost::system::error_code & ec)
            {
                assert(segment < segments_.size());
                if (segment < segments_.size()) {
                    if (begin >= segments_[segment].begin) {
                        segments_[segment].begin = begin;
                        ec = boost::system::error_code();
                    } else {
                        ec = framework::system::logic_error::invalid_argument;
                    }
                } else {
                    ec = framework::system::logic_error::out_of_range;
                }
            }

            void set_segment_max_try(
                size_t segment, 
                size_t max_try, 
                boost::system::error_code & ec)
            {
                assert(segment < segments_.size());
                if (segment < segments_.size()) {
                    segments_[segment].max_try = max_try;
                    ec = boost::system::error_code();
                } else {
                    ec = framework::system::logic_error::out_of_range;
                }
            }

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

            boost::system::error_code poll(
                boost::system::error_code & ec)
            {
                if (source_closed_) {
                    return ec = boost::asio::error::eof;
                } else {
                    return source().poll_read(ec);
                }
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

            boost::uint32_t get_segment_num_try(
                boost::uint32_t index,
                boost::system::error_code &ec) const
            {
                return segments_[write_.segment].num_try;
            }

            boost::system::error_code cancel(
                boost::system::error_code & ec)
            {
                source_error_ = boost::asio::error::operation_aborted;
                return source().cancel_segment(write_.segment, ec);
            }

            boost::system::error_code close(
                boost::system::error_code & ec)
            {
                return close_segment(ec);
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
                    } else if (source().is_open(ec)) {
                        if (segments_[write_.segment].total_state == Segment::not_init) {
                            segments_[write_.segment].total = source().total(ec);
                            if (ec) {
                                segments_[write_.segment].total = boost::uint64_t(-1);
                                segments_[write_.segment].total_state = Segment::not_exist;
                            } else {
                                segments_[write_.segment].total_state = Segment::is_valid;
                                write_.seg_end = write_.seg_beg + segments_[write_.segment].total;
                                if (write_hole_.this_end >= write_.seg_end)
                                    write_hole_.this_end = write_.seg_end;
                                if (read_.segment == write_.segment)
                                    read_.seg_end = write_.seg_end;
                            }
                        }
                        framework::timer::TimeCounter tc;
                        size_t bytes_transferred = boost::asio::read(
                            source(), write_buffer(amount), boost::asio::transfer_at_least(amount), ec);
                        if (tc.elapse() > 10) {
                            LOG_S(framework::logger::Logger::kLevelDebug, 
                                "[prepare] read elapse: " << tc.elapse() 
                                << " bytes_transferred: " << bytes_transferred);
                        }
                        increase_download_byte(bytes_transferred);
                        move_front(write_, bytes_transferred);
                        if (ec && !source().continuable(ec)) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "[prepare] read_some: " << ec.message() << 
                                " --- failed " << segments_[write_.segment].num_try << " times");
                            if (ec == boost::asio::error::eof) {
                                LOG_S(framework::logger::Logger::kLevelDebug, 
                                    "[prepare] read eof, write_.offset: " << write_.offset
                                    << " write_hole_.this_end: " << write_hole_.this_end);
                            }
                        }
                        if (data_end_ < write_.offset)
                            data_end_ = write_.offset;
                    } else {
                        if (!source().continuable(ec)) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "[prepare] open_segment: " << ec.message() << 
                                " --- failed " << segments_[write_.segment].num_try << " times");
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
                    if (source().continuable(ec)) {
                        size_t time_block = get_zero_interval();
                        if (time_block_ < time_block) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "source.read_some: blocked " << time_block << " seconds");
                        }
                        time_block_ = time_block;
                        if (time_out_ > 0 && time_block_ > time_out_) {
                            LOG_S(framework::logger::Logger::kLevelAlarm, 
                                "source.read_some: timeout" << 
                                " --- failed " << segments_[write_.segment].num_try << " times");
                            reset_zero_interval();
                            close_segment(ec);
                            open_segment(false, ec);
                        } else {
                            break;
                        }
                    } else if (ec == boost::asio::error::eof) {
                        if (write_.offset >= write_hole_.this_end) {
                            // 本段结束，尝试下一段
                            reset_zero_interval();
                            time_block_ = 0;
                            open_next(ec);
                            if (ec == boost::asio::error::eof) // 没有下一段了
                                break;
                        } else if (segments_[write_.segment].total_state == Segment::not_exist) {
                            // 本段没有结束，有可能没有指定分段大小，将猜测分段大小
                            segments_[write_.segment].total_state = Segment::by_guess;
                            write_.seg_end = write_.offset;
                            write_hole_.this_end = write_.offset;
                            if (read_.segment == write_.segment)
                                read_.seg_end = write_.seg_end;
                            if (write_hole_.this_end >= write_.seg_end)
                                write_hole_.this_end = write_.seg_end;
                            segments_[write_.segment].total = write_.seg_end - write_.seg_beg;
                            LOG_S(framework::logger::Logger::kLevelInfor, "[prepare] guess segment size " << segments_[write_.segment].total);
                            break;
                        } else {
                            // 不正常结束，看看是否可以重试
                            if (segments_[write_.segment].can_retry()) {
                                reset_zero_interval();
                                close_segment(ec);
                                open_segment(false, ec);
                            } else {
                                source_error_ = ec;
                                boost::system::error_code ec1;
                                close_segment(ec1);
                                break;
                            }
                        }
                    } else if (source().recoverable(ec)) {
                        if (segments_[write_.segment].can_retry()) {
                            reset_zero_interval();
                            close_segment(ec);
                            open_segment(false, ec);
                        } else {
                            source_error_ = ec;
                            boost::system::error_code ec1;
                            close_segment(ec1);
                            break;
                        }
                    } else {
                        source_error_ = ec;
                        boost::system::error_code ec1;
                        close_segment(ec1);
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
                if (ec && !source().continuable(ec)) {
                    LOG_S(framework::logger::Logger::kLevelAlarm, 
                        "[handle_async] read_some: " << ec.message() << 
                        " --- failed " << segments_[write_.segment].num_try << " times");
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
                    if (ec == boost::asio::error::eof) {
                        if (write_.offset >= write_hole_.this_end) {
                            // 本段结束，尝试下一段
                            reset_zero_interval();
                            time_block_ = 0;
                            async_open_next();
                            return;
                        } else if (segments_[write_.segment].total_state == Segment::not_exist) {
                            // 本段没有结束，有可能没有指定分段大小，将猜测分段大小
                            segments_[write_.segment].total_state = Segment::by_guess;
                            write_.seg_end = write_.offset;
                            write_hole_.this_end = write_.offset;
                            if (read_.segment == write_.segment)
                                read_.seg_end = write_.seg_end;
                            if (write_hole_.this_end >= write_.seg_end)
                                write_hole_.this_end = write_.seg_end;
                            segments_[write_.segment].total = write_.seg_end - write_.seg_beg;
                            LOG_S(framework::logger::Logger::kLevelInfor, "[handle_async] guess segment size " << segments_[write_.segment].total);
                        } else {
                            // 不正常结束，看看是否可以重试
                            if (segments_[write_.segment].can_retry()) {
                                reset_zero_interval();
                                close_segment(ec);
                                async_open_segment(false, boost::bind(&BufferList::handle_async, this, _1, 0));
                                return;
                            } else {
                                source_error_ = ec;
                                boost::system::error_code ec1;
                                close_segment(ec1);
                            }
                        }
                    } else if (source().recoverable(ec)) {
                        if (segments_[write_.segment].can_retry()) {
                            reset_zero_interval();
                            close_segment(ec);
                            async_open_segment(false, boost::bind(&BufferList::handle_async, this, _1, 0));
                            return;
                        } else {
                            source_error_ = ec;
                            boost::system::error_code ec1;
                            close_segment(ec1);
                        }
                    } else {
                        source_error_ = ec;
                        boost::system::error_code ec1;
                        close_segment(ec1);
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
                    if (segments_[write_.segment].total_state == Segment::not_init) {
                        segments_[write_.segment].total = source().total(ec);
                        if (ec) {
                            segments_[write_.segment].total = boost::uint64_t(-1);
                            segments_[write_.segment].total_state = Segment::not_exist;
                        } else {
                            segments_[write_.segment].total_state = Segment::is_valid;
                            write_.seg_end = write_.seg_beg + segments_[write_.segment].total;
                            if (write_hole_.this_end >= write_.seg_end)
                                write_hole_.this_end = write_.seg_end;
                            if (read_.segment == write_.segment)
                                read_.seg_end = write_.seg_end;
                        }
                    }
                    boost::asio::async_read(
                        source(), 
                        write_buffer(amount_), 
                        boost::asio::transfer_at_least(amount_), 
                        boost::bind(&BufferList::handle_async, this, _1, _2));
                    return;
                }
                response(ec);
            }

            void response(
                boost::system::error_code const & ec)
            {
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
                if (write_.segment >= segments_.size()) {
                    return ec = boost::asio::error::eof;
                }
                Segment & segment = segments_[write_.segment];
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
                    close_segment(ec);
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
                if (segments_[read_.segment].total_state < Segment::is_valid) {
                    assert(read_.segment == write_.segment);
                    write_.seg_end = write_.offset;
                    read_.seg_end = write_.offset;
                    segments_[read_.segment].total_state = Segment::by_guess;
                    segments_[read_.segment].total = read_.seg_end - read_.seg_beg;
                    LOG_S(framework::logger::Logger::kLevelInfor, "[drop_all] guess segment size " << segments_[write_.segment].total);
                }
                read_seek_to(read_.seg_end, ec) || 
                    offset_to_segment(read_, ec);
                return ec;
            }

        public:
            void clear()
            {
                segments_.clear();
                read_.buffer = buffer_beg();
                read_.offset = 0;
                read_.segment = 0;
                read_.seg_beg = 0;
                write_ = read_;
                read_hole_.this_end = 0;
                read_hole_.next_beg = 0;
                write_hole_.this_end = 0;
                write_hole_.next_beg = 0;
                data_beg_ = 0;
                data_end_ = 0;
                clear_error();
            }

            void clear_error()
            {
                source_error_ = boost::system::error_code();
            }

        public:
            boost::uint32_t read_avail() const
            {
                if (read_.seg_end > write_.offset) {
                    return (boost::uint32_t)(write_.offset - read_.offset);
                } else {
                    return (boost::uint32_t)(read_.seg_end - read_.offset); // total
                }
            }

            boost::uint32_t segment_read_avail(
                size_t segment) const
            {
                if (read_.segment == segment) {
                    return read_avail();
                } else if (segment < read_.segment || segment >= segments_.size()) {
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
                        } else if (segments_[segment].total_state != Segment::is_valid) {
                            return (boost::uint32_t)0;
                        } else if (position.offset + segments_[segment].total < end) {
                            return (boost::uint32_t)segments_[segment].total;
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
                    return (boost::uint64_t)(read_.seg_end - read_.seg_beg); // total
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
                } else if (segment < segments_.size()) {
                    return segments_[segment].begin;
                } else {
                    return boost::uint64_t(0);
                }
            }

            boost::uint64_t segment_size(
                size_t segment) const
            {
                if (segment < segments_.size()) {
                    return segments_[segment].total;
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
                boost::uint64_t beg = segment_read_front(segment);
                boost::uint64_t end = segment_read_back(segment);
                boost::system::error_code ec;
                PositionEx position_beg(segment, beg);
                PositionEx position_end(segment, end);
                offset_of_segment(position_beg, ec);
                offset_of_segment(position_end, ec);
                return read_buffer(position_beg.offset, position_end.offset);
            }

        private:
            Source & source()
            {
                return static_cast<Source &>(*this);
            }

        private:
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
                if (write_.offset != write_offset) {
                    // close source for open from new offset
                    boost::system::error_code ec1;
                    close_segment(ec1);
                }
                offset_to_segment(write_, ec);
                offset_to_segment(read_, ec);
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

            boost::system::error_code open_next(
                boost::system::error_code & ec)
            {
                boost::uint64_t next_offset = read_write_hole(write_hole_.next_beg, write_hole_);
                close_segment(ec);
                move_front_to(write_, next_offset);
                return open_segment(true, ec);
            }

            void async_open_next()
            {
                boost::uint64_t next_offset = read_write_hole(write_hole_.next_beg, write_hole_);

                boost::system::error_code ec;
                close_segment(ec);
                move_front_to(write_, next_offset);
                async_open_segment(true, boost::bind(&BufferList::handle_async, this, _1, 0));
            }

            boost::system::error_code open_segment(
                bool reset_num_try, 
                boost::system::error_code & ec)
            {
                if (offset_to_segment(write_, ec)) {
                    write_hole_.this_end = write_hole_.next_beg = write_.offset;
                    return ec;
                }
                if (write_.segment >= segments_.size() || write_.offset >= seek_end_) {
                    write_hole_.this_end = write_hole_.next_beg = write_.offset;
                    return ec = boost::asio::error::eof;
                }
                if (reset_num_try) {
                    segments_[write_.segment].num_try = 0;
                }
                ++segments_[write_.segment].num_try;
                // W     e^b    e----b      e---
                // 如果当这个分段不能完全填充当前空洞，会切分出一个小空洞，需要插入
                boost::uint64_t end = write_.seg_end;
                if (end > seek_end_)
                    end = seek_end_;
                if (end < write_hole_.this_end) {
                    if (write_write_hole(end, write_hole_) == boost::uint64_t(-1))
                        data_end_ = write_.offset;
                    write_hole_.this_end = end;
                    write_hole_.next_beg = end;
                }
                boost::uint64_t write_offset = write_.seg_beg + segments_[write_.segment].begin;
                assert(seek_end_ >= write_offset);
                if (write_.offset < write_offset) {
                    move_front_to(write_, write_offset);
                }
                source().open_segment(write_.segment, write_.offset - write_.seg_beg, 
                    write_hole_.this_end == boost::uint64_t(-1) || write_hole_.this_end == write_.seg_end ? 
                    boost::uint64_t(-1) : write_hole_.this_end - write_.seg_beg, ec);
                if (ec && !source().continuable(ec)) {
                    LOG_S(framework::logger::Logger::kLevelAlarm, 
                        "source.open_segment: " << ec.message() << 
                        " --- failed " << segments_[write_.segment].num_try << " times");
                    boost::system::error_code ec1;
                    source().close_segment(write_.segment, ec1);
                } else {
                    source_closed_ = false;
                }
                return ec;
            }

            void async_open_segment(
                bool reset_num_try, 
                open_response_type const & resp)
            {
                boost::system::error_code ec;
                if (offset_to_segment(write_, ec)) {
                    write_hole_.this_end = write_hole_.next_beg = write_.offset;
                    resp(ec, 0);
                    return;
                }
                if (write_.segment >= segments_.size() || write_.offset >= seek_end_) {
                    write_hole_.this_end = write_hole_.next_beg = write_.offset;
                    ec = boost::asio::error::eof;
                    resp(ec, 0);
                    return;
                }
                if (reset_num_try) {
                    segments_[write_.segment].num_try = 0;
                }
                ++segments_[write_.segment].num_try;
                // W     e^b    e----b      e---
                // 如果当这个分段不能完全填充当前空洞，会切分出一个小空洞，需要插入
                boost::uint64_t end = write_.seg_end;
                if (end > seek_end_)
                    end = seek_end_;
                if (end < write_hole_.this_end) {
                    if (write_write_hole(end, write_hole_) == boost::uint64_t(-1))
                        data_end_ = write_.offset;
                    write_hole_.this_end = end;
                    write_hole_.next_beg = end;
                }
                boost::uint64_t write_offset = write_.seg_beg + segments_[write_.segment].begin;
                assert(seek_end_ >= write_offset);
                if (write_.offset < write_offset) {
                    move_front_to(write_, write_offset);
                }

                source_closed_ = false;

                source().async_open_segment(
                    write_.segment, write_.offset - write_.seg_beg, 
                    write_hole_.this_end == boost::uint64_t(-1) || write_hole_.this_end == write_.seg_end ? 
                    boost::uint64_t(-1) : write_hole_.this_end - write_.seg_beg, 
                    boost::bind(resp, _1, 0));
            }

            boost::system::error_code close_segment(
                boost::system::error_code & ec)
            {
                if (!source_closed_) {
                    source().close_segment(write_.segment, ec);
                    source_closed_ = true;
                } else {
                    ec = boost::system::error_code();
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
                while (true) {
                    LOG_S(framework::logger::Logger::kLevelDebug2, 
                        "read_hole:" << offset << "-" << hole.this_end);
                    if (hole.this_end == 0)
                        break;
                    offset = read_read_hole(hole.next_beg, hole);
                };
                hole = write_hole_;
                offset = write_.offset;
                while (true) {
                    LOG_S(framework::logger::Logger::kLevelDebug2, 
                        "write_hole:" << offset << "-" << hole.this_end);
                    if (hole.next_beg == boost::uint64_t(-1))
                        break;
                    offset = read_write_hole(hole.next_beg, hole);
                };
            }

            boost::uint64_t read_write_hole(
                boost::uint64_t offset, 
                Hole & hole) const
            {
                if (offset > data_end_) {
                    // next_beg失效，实际空洞从data_end_开始
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
                assert((position.segment < segments_.size() && offset <= segments_[position.segment].total)
                     || (position.segment == segments_.size() && offset == 0));
                if ((position.segment >= segments_.size() || offset > segments_[position.segment].total)
                    && (position.segment != segments_.size() || offset != 0))
                        return ec = framework::system::logic_error::out_of_range;
                for (size_t i = 0; i < position.segment; ++i) {
                    if (segments_[i].total_state < Segment::is_valid)
                        return ec = framework::system::logic_error::out_of_range;
                    offset += segments_[i].total;
                }
                position.seg_beg = offset - position.offset;
                position.seg_end = 
                    (position.segment < segments_.size() && 
                    segments_[position.segment].total_state >= Segment::is_valid) ? 
                    position.seg_beg + segments_[position.segment].total : boost::uint64_t(-1);
                position.offset = offset;
                return ec = boost::system::error_code();
            }

            boost::system::error_code offset_to_segment(
                PositionEx & position, 
                boost::system::error_code & ec) const
            {
                boost::uint64_t offset = position.offset;
                size_t segment = 0;
                for (segment = 0; segment < segments_.size() 
                    && segments_[segment].total_state >= Segment::is_valid 
                    && segments_[segment].total <= offset; ++segment)
                    offset -= segments_[segment].total;
                // 增加offset==0，使得position.offset为所有分段总长时，也认为是有效的
                assert(segment < segments_.size() || offset == 0);
                if (segment < segments_.size() || offset == 0) {
                    position.segment = segment;
                    position.seg_beg = position.offset - offset;
                    position.seg_end = 
                        (segment < segments_.size() && 
                        segments_[position.segment].total_state >= Segment::is_valid) ? 
                            position.seg_beg + segments_[position.segment].total : boost::uint64_t(-1);
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

        private:
            size_t max_try_;
            std::vector<Segment> segments_;
            char * buffer_;
            boost::uint32_t buffer_size_;
            boost::uint32_t prepare_size_;
            boost::uint32_t time_block_;
            boost::uint32_t time_out_;
            bool source_closed_;
            boost::system::error_code source_error_;
            boost::uint64_t data_beg_;
            boost::uint64_t data_end_;
            boost::uint64_t seek_end_;
            PositionEx read_;
            Hole read_hole_;
            PositionEx write_;
            Hole write_hole_;
            framework::memory::PrivateMemory memory_;

            boost::uint32_t amount_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

// Buffer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/Buffer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

namespace ppbox
{
    namespace demux
    {

        FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Buffer", 0)

        Buffer::Buffer(
            boost::uint32_t buffer_size)
            : buffer_(NULL)
            , buffer_size_(framework::memory::MemoryPage::align_page(buffer_size))
            , data_beg_(0)
            , data_end_(0)
        {
            buffer_ = (char *)memory_.alloc_block(buffer_size_);
            read_.buffer = buffer_beg();
            read_.offset = 0;
            write_hole_.this_end = boost::uint64_t(-1);
        }

        Buffer::~Buffer()
        {
            if (buffer_)
                memory_.free_block(buffer_, buffer_size_);
        }

        void Buffer::clear()
        {
            reset(0);
        }

        void Buffer::reset(
            boost::uint64_t offset)
        {
            read_ = Position(offset);
            read_.buffer = buffer_beg();
            write_ = read_;

            read_hole_.this_end = offset;
            read_hole_.next_beg = offset;
            write_hole_ = read_hole_;

            data_beg_ = offset;
            data_end_ = offset;
        }

        bool Buffer::seek(
            boost::uint64_t offset)
        {
            LOG_TRACE("seek " << offset);
            if (data_end_ > data_beg_ + buffer_size_)
                data_beg_ = data_end_ - buffer_size_;// 调整data_beg_
            boost::uint64_t write_offset = write_.offset;
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
            LOG_TRACE("after seek " << offset);
            dump();
            return write_offset != write_.offset;
        }

        bool Buffer::next_write_hole(
            Position & pos, 
            Hole & hole)
        {
            boost::uint64_t next_offset = 
                read_write_hole(hole.next_beg, hole);

            if (pos.buffer != NULL) {
                move_front_to(pos, next_offset);
            } else {
                pos.offset = next_offset;
            }
            return true;
        }

        void Buffer::dump()
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

        boost::uint64_t Buffer::read_write_hole(
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

        boost::uint64_t Buffer::write_write_hole(
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

        boost::uint64_t Buffer::read_read_hole(
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

        boost::uint64_t Buffer::write_read_hole(
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

        void Buffer::read(
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

        void Buffer::write(
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

        void Buffer::back_read(
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

        void Buffer::back_write(
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

        // 循环前移
        char * Buffer::buffer_move_front(
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
        char * Buffer::buffer_move_back(
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

        void Buffer::move_back(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_back(position.buffer, offset);
            position.offset -= offset;
        }

        void Buffer::move_front(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_front(position.buffer, offset);
            position.offset += offset;
        }

        void Buffer::move_back_to(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_back(position.buffer, position.offset - offset);
            position.offset = offset;
        }

        void Buffer::move_front_to(
            Position & position, 
            boost::uint64_t offset) const
        {
            position.buffer = buffer_move_front(position.buffer, offset - position.offset);
            position.offset = offset;
        }

        void Buffer::move_to(
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

// Buffer.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_H_

#include <util/buffers//Buffers.h>

#include <framework/memory/PrivateMemory.h>

#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        class Buffer
        {
        private:
            struct Hole
            {
                Hole()
                    : this_end(0)
                    , next_beg(0)
                {
                }

                boost::uint64_t this_end; // 绝对位置
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

                boost::uint64_t offset; // 整部影片的偏移量
                char * buffer;          // 当前物理地址
            };
            /*
                                            offset=500
                                    segment=2   |
            |_____________|_____________|_______|______|_______________|
                         200           400     500    600             800
                                        |              |
                                    seg_beg=400     seg_end=600
            */

        public:
            Buffer(
                boost::uint32_t buffer_size);

            ~Buffer();

        public:
            typedef util::buffers::Buffers<
                boost::asio::const_buffer, 2
            > read_buffer_t;

            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

        public:
            // 写指针偏移
            boost::uint64_t out_position() const
            {
                return write_.offset;
            }

            size_t out_avail() const
            {
                boost::uint64_t end = write_hole_.this_end;
                if (end > write_.offset + buffer_size_)
                    end = write_.offset + buffer_size_;
                return (size_t)(end - write_.offset);
            }

            write_buffer_t prepare()
            {
                boost::uint64_t beg = write_.offset;
                boost::uint64_t end = write_hole_.this_end;
                if (end > beg + buffer_size_)
                    end = beg + buffer_size_;
                return write_buffer(beg, end);
            }

            write_buffer_t prepare(
                boost::uint32_t size)
            {
                boost::uint64_t beg = write_.offset;
                boost::uint64_t end = beg + size;
                if (end > read_.offset + buffer_size_)
                    end = read_.offset + buffer_size_;
                if (end > write_hole_.this_end)
                    end = write_hole_.this_end;
                return write_buffer(beg, end);
            }

            bool commit(
                size_t size)
            {
                move_front(write_, size);
                assert(write_.offset <= read_.offset + buffer_size_ 
                    && write_.offset <= write_hole_.this_end);
                if (write_.offset <= read_.offset + buffer_size_ 
                    && write_.offset <= write_hole_.this_end) {
                    return true;
                }
                move_back(write_, size);
                return false;
            }

        public:
            // 读指针偏移
            boost::uint64_t in_position() const
            {
                return read_.offset;
            }

            boost::uint64_t in_avail() const
            {
                return write_.offset- read_.offset;
            }

            boost::uint64_t data_begin() const
            {
                return data_beg_;
            }

            boost::uint64_t data_end() const
            {
                return data_end_;
            }

            read_buffer_t data()
            {
                return read_buffer(read_.offset, write_.offset);
            }

            read_buffer_t data(
                boost::uint32_t size)
            {
                boost::uint64_t beg = read_.offset;
                boost::uint64_t end = beg + size;
                if (end > write_.offset)
                    end = write_.offset;
                return read_buffer(beg, end);
            }

            bool consume(
                size_t size)
            {
                move_front(read_, size);
                assert(read_.offset <= write_.offset);
                if (read_.offset <= write_.offset) {
                    return true;
                }
                move_back(read_, size);
                return false;
            }

        public:
            bool check_hole(
                boost::system::error_code & ec)
            {
                if (write_.offset < write_hole_.this_end && write_.offset < read_.offset + buffer_size_)
                    return false;
                if (write_.offset >= read_.offset + buffer_size_) {
                    ec = boost::asio::error::no_buffer_space;
                } else {
                    ec = boost::asio::error::eof;
                }
                return true;
            }

            boost::uint64_t write_hole_size()
            {
                return write_hole_.this_end == boost::uint64_t(-1) 
                    ? boost::uint64_t(-1) : write_hole_.this_end - write_.offset;
            }

            boost::uint64_t next_write_hole()
            {
                next_write_hole(write_, write_hole_);
                return write_hole_size();
            }

            // 返回是否移动了write指针
            bool seek(
                boost::uint64_t offset);

            void clear();

            void reset(
                boost::uint64_t offset);

        private:
            void dump();

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

        protected:
            char const * read_buffer(
                boost::uint64_t & beg, 
                boost::uint64_t cur, 
                boost::uint64_t & end) const
            {
                char const * ptr = buffer_move_front(read_.buffer, beg - read_.offset);
                boost::uint64_t buf_end = beg + (boost::uint32_t)(buffer_end() - ptr);
                if (cur < buf_end) {
                    if (end > buf_end)
                        end = buf_end;
                } else {
                    ptr = buffer_beg();
                    beg = buf_end;
                }
                return ptr;
            }

            read_buffer_t read_buffer(
                boost::uint64_t beg, 
                boost::uint64_t end) const
            {
                boost::asio::const_buffer buffers[2];
                if (end == beg)
                    return read_buffer_t();
                char const * buffer = buffer_move_front(read_.buffer, beg - read_.offset);
                if (end - beg <= (boost::uint32_t)(buffer_end() - buffer)) {
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
                if (end - beg <= (boost::uint32_t)(buffer_end() - buffer)) {
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

        private:
            bool next_write_hole(
                Position & pos, 
                Hole & hole);

            // 循环前移
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

            // 循环后移
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

        private:
            boost::uint64_t read_write_hole(
                boost::uint64_t offset, 
                Hole & hole) const;

            boost::uint64_t write_write_hole(
                boost::uint64_t offset, 
                Hole hole);

            boost::uint64_t read_read_hole(
                boost::uint64_t offset, 
                Hole & hole) const;

            boost::uint64_t write_read_hole(
                boost::uint64_t offset, 
                Hole hole);

            void read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void * dst) const;

            void write(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void const * src);

            void back_read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void * dst) const;

            void back_write(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void const * src);

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

        private:
            framework::memory::PrivateMemory memory_;
            char * buffer_;
            boost::uint32_t buffer_size_;   // buffer_ 分配的大小

            boost::uint64_t data_beg_;
            boost::uint64_t data_end_;

            Position read_;
            Hole read_hole_;
            Position write_;
            Hole write_hole_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

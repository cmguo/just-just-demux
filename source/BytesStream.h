// BytesStream.h

#ifndef _PPBOX_DEMUX_SOURCE_BYTES_STREAM_H_
#define _PPBOX_DEMUX_SOURCE_BYTES_STREAM_H_

#include <util/buffers/BufferSize.h>
#include <util/buffers/StlBuffer.h>

#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        template <typename Buffer>
        class BytesStream
            : public util::buffers::StlStream<boost::uint8_t>
        {
        public:
            typedef typename Buffer::read_buffer_t read_buffer_t;

            typedef typename read_buffer_t::const_iterator const_iterator;

            typedef typename util::buffers::StlBuffer<
                BytesStream, util::buffers::detail::_read> buffer_type;

        public:
            BytesStream(
                Buffer & buffer)
                : buffer_(buffer)
                , size_(0)
                , iter_(buffers_.begin())
                , pos_(0)
                , end_(0)
                , buf_(*this)
                , ec_(boost::asio::error::would_block)
            {
                setg(NULL, NULL, NULL);
            }

        public:
            boost::system::error_code error() const
            {
                return ec_;
            }

            void more(
                boost::uint32_t amount = 0)
            {
                prepare(amount);

                update_new();
            }

            void drop()
            {
                Checker ck(*this);
                pos_type pos = pos_ + off_type(gptr() - eback());
                off_type off = pos + off_type(size_) - end_;
                assert(off >= 0);
                buffer_.drop(off, ec_);

                update();

                iter_ = buffers_.begin();
                assert(gptr() == egptr() 
                    || gptr() == (boost::uint8_t *)boost::asio::buffer_cast<boost::uint8_t const *>(*iter_));
                if (iter_ != buffers_.end()) {
                    buf_ = *iter_;
                } else {
                    setg(NULL, NULL, NULL);
                }
                pos_ = pos;
            }

            void drop_all()
            {
                Checker ck(*this);
                buffer_.drop_all(ec_);

                update();

                iter_ = buffers_.begin();
                if (iter_ != buffers_.end()) {
                    buf_ = *iter_;
                } else {
                    setg(NULL, NULL, NULL);
                }
                pos_ = 0;
                end_ = size_;
            }

            void update_new()
            {
                Checker ck(*this);

                std::size_t iter_dist = std::distance(buffers_.begin(), iter_);
                std::size_t buf_size = iter_ != buffers_.end() ? boost::asio::buffer_size(*iter_) : 0;
                boost::uint32_t size = size_;

                update();

                assert(size_ >= size);
                end_ += (size_ - size);

                iter_ = buffers_.begin();
                std::advance(iter_, iter_dist);
                std::size_t buf_size2 = iter_ != buffers_.end() ? boost::asio::buffer_size(*iter_) : 0;
                assert(buf_size2 >= buf_size);
                if (buf_size) {
                    if (buf_size2 > buf_size) {
                        buf_.commit(buf_size2 - buf_size);
                    }
                } else {
                    // 原先是空BufferList，Buffer是在后面追加的假定不成立
                    if (iter_ != buffers_.end())
                        buf_ = *iter_;
                }
            }

        private:
            void prepare(
                boost::uint32_t amount)
            {
                if (ec_ && ec_ != boost::asio::error::would_block)
                    return;

                buffer_.prepare_at_least(amount, ec_);
            }

            void update()
            {
                buffers_ = buffer_.read_buffer();
                size_ = util::buffers::buffer_size(buffers_);
            }

        private:
            struct Checker
            {
                Checker(
                    BytesStream const & stream)
                    : stream_(stream)
                {
                    stream_.check();
                }

                ~Checker()
                {
                    stream_.check();
                }

            private:
                BytesStream const & stream_;
            };

            void check() const
            {
                pos_type pos = end_ - off_type(size_);
                assert(iter_ != buffers_.end() || buffers_.empty());
                for (const_iterator i = buffers_.begin(); i != buffers_.end(); ++i) {
                    if (i == iter_) {
                        assert(pos == pos_);
                        assert(eback() == (boost::uint8_t *)boost::asio::buffer_cast<boost::uint8_t const *>(*i));
                        size_t size = boost::asio::buffer_size(*i);
                        assert((size_t)(egptr() - eback()) == size);
                        break;
                    }
                    pos += boost::asio::buffer_size(*i);
                }
            }

            virtual int_type underflow()
            {
                Checker ck(*this);
                pos_type pos = pos_ + gptr() - eback();
                if (pos < end_) {
                    pos_ += boost::asio::buffer_size(*iter_);
                    buf_ = *++iter_;
                    return *gptr();
                }
                if (ec_) {
                    return traits_type::eof();
                }
                //TODO:?
                more(1);
                if (pos < end_) {
                    if (gptr() == egptr()) {
                        pos_ += boost::asio::buffer_size(*iter_);
                        buf_ = *++iter_;
                    }
                    return *gptr();
                } else {
                    return traits_type::eof();
                }
            }

            virtual pos_type seekoff(
                off_type off, 
                std::ios_base::seekdir dir,
                std::ios_base::openmode mode)
            {
                if (mode != std::ios_base::in) {
                    return pos_type(-1);
                }
                if (dir == std::ios_base::cur) {
                    pos_type pos = pos_ + gptr() - eback();
                    if (off == 0) {
                        return pos;
                    }
                    pos += off;
                    return seekpos(pos, mode);
                } else if (dir == std::ios_base::beg) {
                    return seekpos(off, mode);
                } else if (dir == std::ios_base::end) {
                    assert(off <= 0);
                    return seekpos(end_ + off, mode);
                } else {
                    return pos_type(-1);
                }
            }

            virtual pos_type seekpos(
                pos_type position, 
                std::ios_base::openmode mode)
            {
                assert(position != pos_type(-1));
                if (mode != std::ios_base::in) {
                    return pos_type(-1);
                }
                if (position < end_ - off_type(size_)) {
                    return pos_type(-1);
                }
                Checker ck(*this);
                if (position > end_) {
                    if (!ec_) {
                        //TODO:?
                        more(position - end_);
                    }
                    if (position > end_) {
                        return pos_type(-1);
                    }
                }
                size_t buf_size = iter_ == 
                    buffers_.end() ? 0 : boost::asio::buffer_size(*iter_);
                if (position >= pos_
                    && position <= pos_ + (off_type)buf_size) {
                        pos_type pos = pos_ + (off_type)(gptr() - eback());
                        gbump(position - pos);
                } else if (position < pos_) {
                    while (position < pos_) {
                        --iter_;
                        pos_ -= boost::asio::buffer_size(*iter_);
                    }
                    buf_ = *iter_;
                    if (position - pos_ > 0)
                        buf_.consume(position - pos_type(pos_));
                } else {
                    pos_ += buf_size;
                    while (position > pos_type(pos_)) {
                        ++iter_;
                        assert(iter_ != buffers_.end());
                        pos_ += boost::asio::buffer_size(*iter_);
                    }
                    pos_ -= boost::asio::buffer_size(*iter_);
                    buf_ = *iter_;
                    if (position - pos_type(pos_) > 0)
                        buf_.consume(position - pos_type(pos_));
                }
                return position;
            }

        private:
            Buffer & buffer_;
            read_buffer_t buffers_; // 有效数据
            boost::uint32_t size_;  // buffers_数据的大小
            const_iterator iter_;   // 当前的内存段
            pos_type pos_;          // 与iter_对应分段的开头
            pos_type end_;          // 有效数据的结尾
            buffer_type buf_;       // 当前的内存段
            boost::system::error_code ec_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BYTES_STREAM_H_

// Mp4BytesStream.h

#ifndef _PPBOX_DEMUX_MP4_MP4_BYTES_STREAM_H_
#define _PPBOX_DEMUX_MP4_MP4_BYTES_STREAM_H_

#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

        template <typename ConstBufferSequence>
        class BufferByteStream
            : public AP4_ByteStream
        {
            typedef typename ConstBufferSequence::const_iterator const_iterator;

        public:
            BufferByteStream(
                ConstBufferSequence const & buffers)
                : offset_(0)
                , size_(0)
                , beg_(buffers.begin())
                , iter_(beg_)
                , end_(buffers.end())
                , nref_(1)
            {
                if (iter_ != end_)
                    buf_ = boost::asio::buffer(*iter_);

                for (const_iterator iter = buffers.begin(); iter != buffers.end(); ++iter) {
                    boost::asio::const_buffer buf(*iter);
                    size_ += boost::asio::buffer_size(buf);
                }
            }

            virtual AP4_Result ReadPartial(
                void * buffer, 
                AP4_Size bytes_to_read, 
                AP4_Size & bytes_read)
            {
                if (offset_ + bytes_to_read > size_)
                    return AP4_ERROR_NOT_ENOUGH_DATA;
                bytes_read = 0;
                while (bytes_to_read) {
                    if (bytes_to_read < boost::asio::buffer_size(buf_)) {
                        memcpy((char *)buffer + bytes_read, boost::asio::buffer_cast<void const *>(buf_), bytes_to_read);
                        buf_ = buf_ + bytes_to_read;
                        bytes_read += bytes_to_read;
                        break;
                    } else {
                        memcpy((char *)buffer + bytes_read, boost::asio::buffer_cast<void const *>(buf_), boost::asio::buffer_size(buf_));
                        bytes_to_read -= boost::asio::buffer_size(buf_);
                        bytes_read += boost::asio::buffer_size(buf_);
                        buf_ = boost::asio::buffer(*++iter_);
                    }
                }
                offset_ += bytes_read;
                return AP4_SUCCESS;
            }

            virtual AP4_Result WritePartial(
                const void * buffer, 
                AP4_Size bytes_to_write, 
                AP4_Size & bytes_written)
            {
                return AP4_ERROR_WRITE_FAILED;
            }

            virtual AP4_Result Seek(
                AP4_Position position)
            {
                if (position > size_) {
                    return AP4_ERROR_NOT_ENOUGH_DATA;
                } else if (position > offset_) {
                    size_t bytes_skip = (size_t)position - offset_;
                    while (bytes_skip) {
                        if (bytes_skip < boost::asio::buffer_size(buf_)) {
                            buf_ = buf_ + bytes_skip;
                            break;
                        } else {
                            bytes_skip -= boost::asio::buffer_size(buf_);
                            buf_ = boost::asio::buffer(*++iter_);
                        }
                    }
                    offset_ = (size_t)position;
                } else if (position == 0) {
                    iter_ = beg_;
                    if (iter_ != end_)
                        buf_ = boost::asio::buffer(*iter_);
                    offset_ = 0;
                } else {
                    Seek(0);
                    Seek(position);
                }
                return AP4_SUCCESS;
            }

            virtual AP4_Result Tell(
                AP4_Position & position)
            {
                position = offset_;
                return AP4_SUCCESS;
            }

            virtual AP4_Result GetSize(
                AP4_LargeSize & size)
            {
                size = size_;
                return AP4_SUCCESS;
            }

            virtual AP4_Result CopyTo(
                AP4_ByteStream& stream, 
                AP4_LargeSize size)
            {
                return AP4_ERROR_READ_FAILED;
            }

            virtual void AddReference()
            {
                ++nref_;
            }

            virtual void Release()
            {
                if (--nref_ == 0)
                    delete this;
            }

        private:
            size_t offset_;
            size_t size_;
            typename ConstBufferSequence::const_iterator beg_;
            typename ConstBufferSequence::const_iterator iter_;
            typename ConstBufferSequence::const_iterator end_;
            boost::asio::const_buffer buf_;
            size_t nref_;
        };

        template <typename ConstBufferSequence>
        static BufferByteStream<ConstBufferSequence> * new_buffer_byte_stream(
            ConstBufferSequence const & buffers)
        {
            return new BufferByteStream<ConstBufferSequence>(buffers);
        }

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_MP4_MP4_BYTES_STREAM_H_

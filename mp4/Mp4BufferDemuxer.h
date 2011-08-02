// Mp4BufferDemuxer.h

#ifndef _PPBOX_DEMUX_MP4_MP4_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_MP4_MP4_BUFFER_DEMUXER_H_

#include "ppbox/demux/mp4/Mp4DemuxerBase.h"

#include <util/buffers/BufferCopy.h>

#include <boost/asio/io_service.hpp>

namespace ppbox
{
    namespace demux
    {

        template <typename Buffer>
        class Mp4BufferDemuxer
            : public Mp4DemuxerBase
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            Mp4BufferDemuxer(
                Buffer & buffer)
                : buffer_(buffer)
                , segment_(size_t(-1))
                , head_size_(0)
                , seek_time_(0)
            {
            }

            boost::system::error_code open(
                boost::uint64_t head_size, 
                boost::uint64_t total_size, 
                boost::system::error_code & ec)
            {
                segment_ = buffer_.add_segment(total_size, ec);
                head_size_ = (boost::uint32_t)head_size;
                //if (buffer_.read_segment() == segment_)
                //    is_open(ec);
                return ec;
            }

            boost::system::error_code open(
                boost::system::error_code & ec)
            {
                segment_ = buffer_.add_segment(ec);
                //if (buffer_.read_segment() == segment_)
                //    is_open(ec);
                return ec;
            }

            void async_open(
                open_response_type const & resp)
            {
                boost::system::error_code ec;
                if (head_valid()) {
                    resp(boost::system::error_code());
                    return;
                }

                assert(segment_ == buffer_.read_segment());

                resp_ = resp;

                handle_open(ec);
            }

            bool is_open(
                boost::system::error_code & ec)
            {
                if (head_valid()) {
                    ec = boost::system::error_code();
                    return true;
                }
                ec = boost::system::error_code();
                assert(segment_ == buffer_.read_segment());
                while (!ec && buffer_.read_avail() < min_head_size(buffer_.read_buffer())) {
                    buffer_.prepare(head_size() - buffer_.read_avail(), ec);
                }
                if (!ec && !set_head(buffer_.read_buffer(), buffer_.segment_size(segment_), ec)) {
                    buffer_.drop_to(head_size(), ec);
                    //buffer_.set_segment_begin(segment_, head_size(), ec);
                    if (seek_time_) {
                        boost::uint64_t offset = seek_to(seek_time_, ec);
                        if (!ec) {
                            buffer_.seek(segment_, offset, ec);
                        }
                    }
                }
                return !ec;
            }

            boost::system::error_code get_head_data(
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec)
            {
                buffer_.prepare_at_least(0, ec);
                boost::uint64_t read_front = buffer_.segment_read_front(segment_);
                boost::uint64_t read_back = buffer_.segment_read_back(segment_);
                if (head_valid()) {
                    if (read_front == 0 && head_size() <= read_back) {
                        data.resize(head_size());
                        util::buffers::buffer_copy(
                            boost::asio::buffer(data), buffer_.segment_read_buffer(segment_), head_size());
                        ec = boost::system::error_code();
                    } else {
                        get_head(data, ec);
                    }
                } else if (read_front == 0) {
                    typename Buffer::read_buffer_t read_buffer = buffer_.segment_read_buffer(segment_);
                    if (min_head_size(read_buffer) <= read_back) {
                        data.resize(head_size());
                        util::buffers::buffer_copy(boost::asio::buffer(data), read_buffer, head_size());
                        ec = boost::system::error_code();
                    } else {
                        ec = boost::asio::error::would_block;
                    }
                } else {
                    ec = boost::asio::error::would_block;
                }
                return ec;
            }

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                buffer_.prepare_at_least(0, ec_buf);
                boost::uint64_t read_front = buffer_.segment_read_front(segment_);
                boost::uint64_t read_back = buffer_.segment_read_back(segment_);
                boost::uint32_t time = 0;
                if (head_valid()) {
                    time = Mp4DemuxerBase::get_end_time(read_back, ec);
                } else if (read_front == 0) {
                    typename Buffer::read_buffer_t read_buffer = buffer_.segment_read_buffer(segment_);
                    if (min_head_size(read_buffer) < read_back) {
                        if (!set_head(read_buffer, buffer_.segment_size(segment_), ec)) {
                            //if (seek_time_) {
                            //    boost::system::error_code ec1;
                            //    boost::uint64_t offset = seek_to(seek_time_, ec1);
                            //    if (!ec1) {
                            //        buffer_.seek(segment_, offset, ec1);
                            //    }
                            //}
                            time = Mp4DemuxerBase::get_end_time(read_back, ec);
                        }
                    } else {
                        ec = boost::asio::error::would_block;
                    }
                } else {
                    ec = boost::asio::error::would_block;
                }
                return time;
            }

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec)
            {
                if (!is_open(ec)) {
                    return 0;
                } else {
                    return Mp4DemuxerBase::get_cur_time(ec);
                }
            }

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec)
            {
                if (!is_open(ec)) {
                } else if (Mp4DemuxerBase::get_sample(sample, ec)) {
                    if (ec == ppbox::demux::error::no_more_sample) {
                        boost::system::error_code ec1;
                        buffer_.drop_all(ec1);
                        if (ec1)
                            ec = ec1;
                    }
#ifndef PPBOX_DEMUX_MP4_TIME_ORDER
#  ifdef PPBOX_DEMUX_RETURN_SKIP_DATA
                } else if (sample.offset > buffer_.read_front()) {
                    boost::system::error_code ec1;
                    put_back_sample(sample, ec1);
                    assert(!ec1);
                    sample.itrack = (boost::uint32_t)-1;
                    //sample.time = (boost::uint32_t)-1;
                    sample.size = (boost::uint32_t)(sample.offset - buffer_.read_front());
                    sample.offset -= sample.size;
                    sample.is_sync = false;
                    buffer_.read(sample.size, sample.data, ec);
#  endif
                } else {
                    buffer_.drop_to(sample.offset, ec) ||
                        buffer_.read(sample.size, sample.data, ec);
                    if (ec) {
                        boost::system::error_code ec1;
                        put_back_sample(sample, ec1);
                        assert(!ec1);
                    }
                }
#else
                } else {
                    buffer_.peek(sample.offset, sample.size, sample.data, ec) ||
                        buffer_.drop_to(Mp4DemuxerBase::min_offset(), ec);
                    if (ec) {
                        boost::system::error_code ec1;
                        put_back_sample(sample, ec1);
                        assert(!ec1);
                    }
                }
#endif
                return ec;
            }

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec)
            {
                if (head_valid()) {
                    boost::uint64_t offset = seek_to(time, ec);
                    if (!ec) {
                        buffer_.seek(segment_, offset, ec);
                    }
                } else {
                    seek_time_ = time;
                    if (head_size_ && seek_time_) { // 知道头部大小，先下载头部
                        buffer_.seek(segment_, 0, head_size_, ec)
                            || is_open(ec);
                    } else {   // 不知道头部大小，从头开始下
                        buffer_.seek(segment_, 0, ec)
                            || is_open(ec);
                    }
                    if (head_valid())
                        time = get_cur_time(ec);
                }
                return ec;
            }

        private:
            void handle_open(
                boost::system::error_code const & ecc)
            {
                boost::system::error_code ec = ecc;

                if (!ec && buffer_.read_avail() < min_head_size(buffer_.read_buffer())) {
                    buffer_.async_prepare(
                        head_size() - buffer_.read_avail(), 
                        boost::bind(&Mp4BufferDemuxer::handle_open, this, _1));
                    return;
                }

                if (!ec && !set_head(buffer_.read_buffer(), buffer_.segment_size(segment_), ec)) {
                    buffer_.drop_to(head_size(), ec);
                    if (seek_time_) {
                        boost::uint64_t offset = seek_to(seek_time_, ec);
                        if (!ec) {
                            buffer_.seek(segment_, offset, ec);
                        }
                    }
                }

                response(ec);
            }

            void response(
                boost::system::error_code const & ec)
            {
                open_response_type resp;
                resp.swap(resp_);
                resp(ec);
            }

        private:
            Buffer & buffer_;
            size_t segment_;
            boost::uint32_t head_size_; // drag 信息中的头部大小
            boost::uint32_t seek_time_; // 分段相对需要seek的时间
            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_MP4_MP4_BUFFER_DEMUXER_H_

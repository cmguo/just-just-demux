// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/source/BytesStream.h"
#include "ppbox/demux/base/DemuxerSource.h"

#include "ppbox/demux/base/BufferDemuxer.h"

#include <framework/timer/Ticker.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        BufferDemuxer::BufferDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            DemuxerSource * source)
            : buffer_(new BufferList(buffer_size, prepare_size, (SourceBase *)source))
            , io_svc_(io_svc)
            , seek_time_(0)
            , root_source_(source)
        {
        }

        BufferDemuxer::~BufferDemuxer()
        {
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
        }

        struct SyncResponse
        {
            SyncResponse(
                boost::system::error_code & ec)
                : ec_(ec)
                , returned_(false)
            {
            }

            void operator()(
                boost::system::error_code const & ec)
            {
                boost::mutex::scoped_lock lock(mutex_);
                ec_ = ec;
                returned_ = true;
                cond_.notify_all();
            }

            void wait()
            {
                boost::mutex::scoped_lock lock(mutex_);
                while (!returned_)
                    cond_.wait(lock);
            }

            boost::system::error_code & ec_;
            bool returned_;
            boost::mutex mutex_;
            boost::condition_variable cond_;
        };

        boost::system::error_code BufferDemuxer::open (
            boost::system::error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(boost::ref(resp));
            resp.wait();
            return ec;
        }

        void BufferDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            create_demuxer(root_source_, buffer_->read_segment(), read_demuxer_, ec);
            create_demuxer(root_source_, buffer_->write_segment(), write_demuxer_, ec);
            handle_async(boost::system::error_code());
        }

        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxerSegment segment = root_source_->demuxer_segment(root_source_->total_segments() - 1);
                    time = segment.duration_offset + segment.duration;
                }
            } else {
                size_t seg_read = buffer_->read_segment().segment;
                if (seg_read < root_source_->total_segments()) {
                    time = read_demuxer_.segment.duration_offset + read_demuxer_.demuxer->get_cur_time(ec);
                } else { // 可能已经播放结束了
                    ec.clear();
                    //Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    DemuxerSegment segment = root_source_->demuxer_segment(root_source_->total_segments() - 1);
                    time = read_demuxer_.segment.duration_offset + read_demuxer_.segment.duration;
                }
            }
            on_error(ec);
        }

        boost::uint32_t BufferDemuxer::get_end_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            tick_on();
            write_demuxer_.stream->more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxerSegment segment = root_source_->demuxer_segment(root_source_->total_segments() - 1);
                    time = segment.duration_offset + segment.duration;
                }
            } else {
                size_t seg_write = buffer_->write_segment().segment;
                if (seg_write < root_source_->total_segments()) {
                    DemuxerSegment segment = root_source_->demuxer_segment(seg_write);
                    if (write_demuxer_.stream->segment().segment != seg_write) {
                        create_demuxer((DemuxerSource *)const_cast<SourceBase *>(buffer_->write_segment().source), buffer_->write_segment(), write_demuxer_, ec);
                    }
                    time = segment.duration_offset + write_demuxer_.demuxer->get_end_time(ec);
                    if (ec == error::file_stream_error) {
                        ec = write_demuxer_.stream->error();
                    }
                } else { // 可能已经下载结束了
                    ec.clear();
                    //Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    DemuxerSegment segment = root_source_->demuxer_segment(root_source_->total_segments() - 1);
                    time = segment.duration_offset + segment.duration;
                }
            }
            ec_buf = write_demuxer_.stream->error();
            on_error(ec);
            return time;
        }

        boost::system::error_code BufferDemuxer::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            // 如果找不到对应的分段，错误码就是source_error::no_more_segment
            ec = source_error::no_more_segment;
            for (size_t i = 0; i < root_source_->total_segments(); ++i) {
                DemuxerSegment segment = root_source_->demuxer_segment(i);
                if (time < segment.duration_offset + segment.duration) {
                    time -= segment.duration_offset;
                    create_demuxer(root_source_, segment, read_demuxer_, ec);
                    boost::uint64_t offset = read_demuxer_.demuxer->seek(time, ec);
                    if (!ec) {
                        seek_time_ = 0;
                        read_demuxer_.stream->seek(offset);
                        time = read_demuxer_.demuxer->get_cur_time(ec);
                    } else {
                        if (segment.head_length && seek_time_) {
                            read_demuxer_.stream->seek(0, segment.head_length);
                        } else {
                            read_demuxer_.stream->seek(0);
                        }
                    }
                    create_demuxer((DemuxerSource *)const_cast<SourceBase *>(buffer_->write_segment().source), buffer_->write_segment().segment, write_demuxer_, ec);
                    time += segment.duration_offset;
                }
            }
            if (ec)
                seek_time_ = time;
            root_source_->on_error(ec);
            on_error(ec);
            return ec;
        }

        boost::system::error_code BufferDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            tick_on();
            read_demuxer_.stream->more(0);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                return ec;
            }
            read_demuxer_.stream->drop();
            while (read_demuxer_.demuxer->get_sample(sample, ec)) {
                if (ec == ppbox::demux::error::file_stream_error
                    || ec == error::no_more_sample) {
                    if (buffer_->read_segment().segment != buffer_->write_segment().segment) {
                        std::cout << "drop_all" << std::endl;
                        read_demuxer_.stream->drop_all();
                        create_demuxer((DemuxerSource *)const_cast<SourceBase *>(buffer_->read_segment().source), buffer_->read_segment().segment, read_demuxer_, ec);
                        boost::uint32_t seek_time = 0;
                        read_demuxer_.demuxer->seek(seek_time, ec);
                        if (!ec) {
                            read_demuxer_.demuxer->get_sample(sample, ec);
                        }
                        if (ec == error::file_stream_error) {
                            ec = read_demuxer_.stream->error();
                        }
                    } else {
                        ec = read_demuxer_.stream->error();
                        break;
                    }
                }
            }
            if (!ec) {
                play_on();
                sample.data.clear();
                for (size_t i = 0; i < sample.blocks.size(); ++i) {
                    buffer_->peek(sample.blocks[i].offset, sample.blocks[i].size, sample.data, ec);
                    if (ec) {
                        on_error(ec);
                        break;
                    }
                }
            } else {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                on_error(ec);
            }
            return ec;
        }

        size_t BufferDemuxer::get_media_count(
            boost::system::error_code & ec)
        {
            size_t n = read_demuxer_.demuxer->get_media_count(ec);
            on_error(ec);
            return n;
        }

        boost::system::error_code BufferDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
        {
            read_demuxer_.demuxer->get_media_info(index, info, ec);
            on_error(ec);
            return ec;
        }

        boost::uint32_t BufferDemuxer::get_duration(
            boost::system::error_code & ec)
        {
            boost::uint32_t d = read_demuxer_.demuxer->get_duration(ec);
            on_error(ec);
            return d;
        }

        boost::system::error_code BufferDemuxer::insert_source(
            boost::uint32_t time, 
            DemuxerSource * src, 
            DemuxerSource * dest, 
            boost::system::error_code & ec)
        {
            for (size_t i = 0; i < src->total_segments(); i++) {
                DemuxerSegment segment = src->demuxer_segment(i);
                if (time < segment.duration_offset + segment.duration) {
                    if (i > buffer_->write_segment().segment) {
                        
                    }
                }
            }
            return ec;
        }

        void BufferDemuxer::handle_async(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (!ec) {
                read_demuxer_.stream->update_new();
                if (read_demuxer_.demuxer->is_open(ec)) {
                    read_demuxer_.stream->drop();
                    if (seek_time_) {
                        boost::uint64_t offset = read_demuxer_.demuxer->seek(seek_time_, ec);
                        if (!ec) {
                            read_demuxer_.stream->seek(offset);
                        }
                    }
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    if (read_demuxer_.demuxer->head_size() && buffer_->read_back() < read_demuxer_.demuxer->head_size()) {
                        buffer_->async_prepare(
                            read_demuxer_.demuxer->head_size() - buffer_->read_back(), 
                            boost::bind(&BufferDemuxer::handle_async, this, _1));
                    } else {
                        buffer_->async_prepare_at_least(0, 
                            boost::bind(&BufferDemuxer::handle_async, this, _1));
                    }
                    return;
                }
            }
            open_end();
            response(ec);
        }

        void BufferDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

        void BufferDemuxer::create_demuxer(
            DemuxerSource & source, 
            SegmentPosition const & segment, 
            DemuxerInfo & demuxer, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (&source == &read_demuxer_.stream->source() && segment == read_demuxer_.stream->segment()) {
                demuxer.stream = read_demuxer_.stream;
                demuxer.demuxer = read_demuxer_.demuxer;
            } else if (&source == &write_demuxer_.stream->source() && segment = write_demuxer_.stream->segment()) {
                demuxer.stream = write_demuxer_.stream;
                demuxer.demuxer = write_demuxer_.demuxer;
            } else {
                demuxer.segment = source.demuxer_segment(segment.segment);
                demuxer.stream.reset(new BytesStream(*buffer_, source, segment));
                demuxer.demuxer.reset(create_demuxer(demuxer.segment.demuxer_type, *demuxer.stream));
                demuxer.demuxer->open(ec);
            }
        }

        void BufferDemuxer::tick_on()
        {
            if (ticker_->check()) {
                update_stat();
            }
        }

        void BufferDemuxer::update_stat()
        {
            error_code ec;
            error_code ec_buf;
            boost::uint32_t buffer_time = get_buffer_time(ec, ec_buf);
            set_buf_time(buffer_time);
        }

        void BufferDemuxer::on_extern_error(
            boost::system::error_code const & ec)
        {
            extern_error_ = ec;
        }

        boost::system::error_code BufferDemuxer::get_sample_buffered(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (state_ == buffering && play_position_ > seek_position_ + 30000) {
                boost::system::error_code ec_buf;
                boost::uint32_t time = get_buffer_time(ec, ec_buf);
                if (ec && ec != boost::asio::error::would_block) {
                } else {
                    //if (time < 2000 && ec_buf != boost::asio::error::eof) {
                    //    ec = ec_buf;
                    //}
                    if (ec_buf
                        && ec_buf != boost::asio::error::would_block
                        && ec_buf != boost::asio::error::eof) {
                        ec = ec_buf;
                    } else {
                        if (time < 2000 && ec_buf != boost::asio::error::eof) {
                            ec = boost::asio::error::would_block;
                        } else {
                            ec.clear();
                        }
                    }
                }
            }
            if (!ec) {
                get_sample(sample, ec);
            }
            return ec;
        }

        boost::uint32_t BufferDemuxer::get_buffer_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            if (need_seek_time_) {
                seek_position_ = get_cur_time(ec);
                if (ec) {
                    if (ec == boost::asio::error::would_block) {
                        ec_buf = boost::asio::error::would_block;
                    }
                    return 0;
                }
                need_seek_time_ = false;
                play_position_ = seek_position_;
            }
            boost::uint32_t buffer_time = get_end_time(ec, ec_buf);
            buffer_time = buffer_time > play_position_ ? buffer_time - play_position_ : 0;
            //set_buf_time(buffer_time);
            // 直接赋值，减少输出日志，set_buf_time会输出buf_time
            buffer_time_ = buffer_time;
            return buffer_time;
        }

        void BufferDemuxer::on_error(
            boost::system::error_code &ec)
        {
            
        }

    } // namespace demux
} // namespace ppbox

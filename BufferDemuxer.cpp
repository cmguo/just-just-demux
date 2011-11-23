// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/BufferDemuxer.h"
#include "ppbox/demux/asf/AsfDemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/source/BytesStream.h"
#include "ppbox/demux/source/SourceBase.h"
using namespace framework::logger;

using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        BufferDemuxer::BufferDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            SourceBase * segmentbase)
            : buffer_(new BufferList(buffer_size, prepare_size, segmentbase))
            , seek_time_(0)
            , root_source_(segmentbase)
            , read_demuxer_(NULL)
            , write_demuxer_(NULL)
        {
        }

        BufferDemuxer::~BufferDemuxer()
        {
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
            if (read_demuxer_ == write_demuxer_) {
                delete read_stream_;
                read_stream_ = NULL;
                write_stream_ = NULL;
                delete read_demuxer_;
                read_demuxer_ = NULL;
                write_demuxer_ = NULL;
            } else {
                if (read_demuxer_) {
                    delete read_stream_;
                    read_stream_ = NULL;
                    delete read_demuxer_;
                    read_demuxer_ = NULL;
                }
                if (write_demuxer_ ) {
                    delete write_stream_;
                    write_stream_ = NULL;
                    delete write_demuxer_;
                    write_demuxer_ = NULL;
                }
            }
        }

        boost::system::error_code BufferDemuxer::open (
            boost::system::error_code & ec)
        {
            open_beg();
            create_demuxer(ec);
            read_stream_->more(0);
            if (!ec) {
                read_stream_->drop();
            } else if (ec == ppbox::demux::error::file_stream_error) {
                ec = read_stream_->error();
                if (ec == boost::asio::error::eof) {
                    read_demuxer_->open(ec);
                }
            }
            return ec;
        }

        void BufferDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            create_demuxer(ec);
            handle_async(boost::system::error_code());
        }

        bool BufferDemuxer::is_open(
            boost::system::error_code & ec)
        {
            bool result = read_demuxer_->is_open(ec);
            if (result) {
                ec = boost::system::error_code();
                return result;
            }
            Segment read_segment = (* root_source_)[buffer_->read_segment()];
            ec.clear();
            while (!ec && buffer_->read_avail() < read_segment.head_length) {
                read_stream_->more(read_segment.head_length - buffer_->read_avail());
            }
            if (!ec && read_demuxer_->is_open(ec)) {
                open_end();
                if (seek_time_) {
                    boost::uint64_t offset = read_demuxer_->seek(seek_time_, ec);
                    if (!ec) {
                        read_stream_->seek(offset);
                    }
                }
            }
            return !ec;
        }

        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == would_block) {
                    Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    time = segment.duration_offset + segment.duration;
                }
            } else {
                size_t seg_read = buffer_->read_segment();
                if (seg_read < root_source_->total_segments()) {
                    Segment const & segment = (* root_source_)[seg_read];
                    time = segment.duration_offset + read_demuxer_->get_cur_time(ec);
                } else { // 可能已经播放结束了
                    ec.clear();
                    Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    time = segment.duration_offset + segment.duration;
                }
            }
            on_error(ec);
        }

        boost::uint32_t BufferDemuxer::get_end_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            tick_on();
            write_stream_->more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == would_block) {
                    Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    time = segment.duration_offset + segment.duration;
                }
            } else {
                size_t seg_write = buffer_->write_segment();
                if (seg_write < root_source_->total_segments()) {
                    Segment const & segment = (* root_source_)[seg_write];
                    if (write_stream_->segment() != seg_write) {
                        create_demuxer(buffer_->write_source(), buffer_->write_segment(), write_stream_, write_demuxer_, ec);
                    }
                    time = segment.duration_offset + write_demuxer_->get_end_time(ec);
                    if (ec == error::file_stream_error) {
                        ec = write_stream_->error();
                    }
                } else { // 可能已经下载结束了
                    ec.clear();
                    Segment const & segment = (* root_source_)[root_source_->total_segments() - 1];
                    time = segment.duration_offset + segment.duration;
                }
            }
            ec_buf = write_stream_->error();
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
                if (time < (* root_source_)[i].duration_offset + (* root_source_)[i].duration) {
                    Segment segment = (* root_source_)[i];
                    on_event(DemuxerEventType::seek_segment, & i, ec);
                    time -= segment.duration_offset;
                    create_demuxer(*root_source_, i, read_stream_, read_demuxer_, ec);
                    boost::uint64_t offset = read_demuxer_->seek(time, ec);
                    if (!ec) {
                        seek_time_ = 0;
                        read_stream_->seek(offset);
                    } else {
                        if (segment.head_length && seek_time_) {
                            read_stream_->seek(0, segment.head_length);
                        } else {
                            read_stream_->seek(0);
                        }
                        if (is_open(ec)) {
                            time = read_demuxer_->get_cur_time(ec);
                        }
                    }
                    create_demuxer(buffer_->write_source(), buffer_->write_segment(), write_stream_, write_demuxer_, ec);
                    time += segment.duration_offset;
                    if (seek_time_ != (boost::uint32_t)-1) {
                        on_event(DemuxerEventType::seek_statistic, & time, ec);
                    }
                }
            }
            if (ec)
                seek_time_ = time;
            root_source_.on_error(ec);
            on_error(ec);
            return ec;
        }

        boost::system::error_code BufferDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            tick_on();
            read_stream_->more(0);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == would_block) {
                    block_on();
                }
                return ec;
            }
            read_stream_->drop();
            while (read_demuxer_->get_sample(sample, ec)) {
                if (ec == ppbox::demux::error::file_stream_error
                    || ec == error::no_more_sample) {
                    if (buffer_->read_segment() != buffer_->write_segment()) {
                        std::cout << "drop_all" << std::endl;
                        read_stream_->drop_all();
                        create_demuxer(buffer_.read_source(), buffer_->read_segment(), read_stream_, read_demuxer_, ec);
                        boost::uint32_t seek_time = 0;
                        read_demuxer_->seek(seek_time, ec);
                        if (!ec) {
                            read_demuxer_->get_sample(sample, ec);
                        }
                        if (ec == error::file_stream_error) {
                            ec = read_stream_->error();
                        }
                    } else {
                        ec = read_stream_->error();
                        break;
                    }
                }
            }
            if (!ec) {
                play_on();
                sample.data.clear();
                for (size_t i = 0; i < sample.blocks.size(); ++i) {;
                    buffer_->peek((*iter).offset, (*iter).size, sample.data, ec);
                    if (ec) {
                        on_error(ec);
                        break;
                    }
                }
            } else {
                if (ec == would_block) {
                    block_on();
                }
                on_error(ec);
            }
            return ec;
        }

        size_t BufferDemuxer::get_media_count(
            boost::system::error_code & ec)
        {
            size_t n = read_demuxer_->get_media_count(ec);
            on_error(ec);
            return n;
        }

        boost::system::error_code BufferDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
        {
            read_demuxer_->get_media_info(index, info, ec);
            on_error(ec);
            return ec;
        }

        boost::uint32_t BufferDemuxer::get_duration(
            boost::system::error_code & ec)
        {
            boost::uint32_t d = read_demuxer_->get_duration(ec);
            on_error(ec);
            return d;
        }

        boost::system::error_code BufferDemuxer::insert_source(
            boost::uint32_t time, 
            SourceBase * src, 
            SourceBase * dest, 
            boost::system::error_code & ec)
        {
            for (size_t i = 0; i < src->total_segments(); i++) {
                if (time < (*src)[i].duration_offset + (*src)[i].duration) {
                    if (i > buffer_->write_segment()) {
                        
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
                read_stream_->update_new();
                if (read_demuxer_->is_open(ec)) {
                    read_stream_->drop();
                    if (seek_time_) {
                        boost::uint64_t offset = read_demuxer_->seek(seek_time_, ec);
                        if (!ec) {
                            read_stream_->seek(offset);
                        }
                    }
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    if (read_demuxer_->head_size() && buffer_->read_avail() < read_demuxer_->head_size()) {
                        buffer_->async_prepare(
                            read_demuxer_->head_size() - buffer_->read_avail(), 
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

        DemuxerBase * BufferDemuxer::create_demuxer(
            DemuxerType::Enum demuxer_type,
            BytesStream * buf)
        {
            switch(demuxer_type)
            {
                case DemuxerType::mp4:
                    return new Mp4DemuxerBase(*buf);
                case DemuxerType::asf:
                    return new AsfDemuxerBase(*buf);
                case DemuxerType::flv:
                    return new FlvDemuxerBase(*buf);
                default:
                    assert(0);
                    return NULL;
            }
        }

        void BufferDemuxer::create_demuxer(
            SourceBase & source, 
            size_t segment, 
            StreamPointer & stream, 
            DemuxerPointer & demuxer, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (&source == &read_stream_->source() && segment == read_stream_->segment()) {
                stream = read_stream_;
                demuxer = read_demuxer_;
            } else if (&source == &write_stream_->source() && segment = write_stream_->segment()) {
                stream = write_stream_;
                demuxer = write_demuxer_;
            } else {
                stream.reset(new BytesStream(*buffer_, source, segment));
                demuxer.reset(create_demuxer(source[segment].demuxer_type, *stream));
                demuxer->open(ec);
            }
        }

    } // namespace demux
} // namespace ppbox

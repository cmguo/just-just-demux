// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/BufferDemuxer.h"
#include "ppbox/demux/asf/AsfDemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/source/BytesStream.h"
#include "ppbox/demux/source/SegmentsBase.h"
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
            SegmentsBase * segmentbase)
            : buffer_(new BufferList(buffer_size, prepare_size, segmentbase))
            , seek_time_(0)
            , segments_(segmentbase)
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
            Segment read_segment = (* segments_)[buffer_->read_segment()];
            ec.clear();
            while (!ec && buffer_->read_avail() < read_segment.head_length) {
                read_stream_->more(read_segment.head_length - buffer_->read_avail());
            }
            if (!ec && read_demuxer_->is_open(ec)) {
                if (seek_time_) {
                    boost::uint64_t offset = read_demuxer_->seek(seek_time_, ec);
                    if (!ec) {
                        read_stream_->seek(offset);
                    }
                }
            }
            return !ec;
        }

        boost::uint32_t BufferDemuxer::get_end_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            write_stream_->more(0);
            ec_buf = write_stream_->error();
            boost::uint32_t time = 0;
            size_t seg_write = buffer_->write_segment();
            Segment segment = (* segments_)[seg_write];
            if (seg_write < segments_->total_segments()) {
                if (write_stream_->segment() != seg_write) {
                    create_write_demuxer(ec);
                }
                boost::uint32_t buffer_time = write_demuxer_->get_end_time(ec);
                time = segment.duration_offset + buffer_time;
                if (ec == error::file_stream_error) {
                    ec = write_stream_->error();
                }
            } else {
                ec.clear();
                ec_buf = boost::asio::error::eof;
                time = (*segments_)[segments_->total_segments() - 1].duration_offset + (*segments_)[segments_->total_segments() - 1].duration;
            }
            return time;
        }

        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            size_t seg_read = buffer_->read_segment();
            if (seg_read < segments_->total_segments()) {
                return (*segments_)[seg_read].duration_offset + read_demuxer_->get_cur_time(ec);
            } else {
                ec.clear();
                return (*segments_)[segments_->total_segments() - 1].duration_offset + (*segments_)[segments_->total_segments() - 1].duration;
            }
        }

        boost::system::error_code BufferDemuxer::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            for (size_t i = 0; i < segments_->total_segments(); ++i) {
                if (time < (* segments_)[i].duration_offset + (* segments_)[i].duration) {
                    Segment segment = (* segments_)[i];
                    on_event(DemuxerEventType::seek_segment, & i, ec);
                    time -= segment.duration_offset;
                    create_segment_demuxer(i, ec);
                    boost::uint64_t offset = read_demuxer_->seek(time, ec);
                    if (!ec) {
                        read_stream_->seek(offset);
                    } else {
                        seek_time_ = time;
                        if (segment.head_length && seek_time_) {
                            read_stream_->seek(0, segment.head_length);
                        } else {
                            read_stream_->seek(0);
                        }
                        if (is_open(ec)) {
                            time = read_demuxer_->get_cur_time(ec);
                        }
                    }
                    time += segment.duration_offset;
                    if (seek_time_ != (boost::uint32_t)-1) {
                        seek_time_ = 0;
                        on_event(DemuxerEventType::seek_statistic, & time, ec);
                    }
                    return ec;
                } else {
                    ec = ppbox::demux::error::bad_file_format;
                }
            }
            return ec;
        }

        boost::system::error_code BufferDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            read_stream_->more(0);
            read_demuxer_->get_sample(sample, ec);
            if (!ec) {
                read_stream_->drop();
            } else if (ec == error::file_stream_error
                || ec == error::no_more_sample) {
                if (buffer_->read_segment() != buffer_->write_segment()) {    // 当前分段已经下载完成
                    std::cout << "drop_all" << std::endl;
                    read_stream_->drop_all();
                    size_t seg_read = buffer_->read_segment();
                    LOG_S(Logger::kLevelDebug, 
                        "segment: " << seg_read << 
                        " duration: " << (*segments_)[seg_read].duration);
                    create_read_demuxer(ec);
                    on_event(DemuxerEventType::seek_segment, &seg_read, ec);
                    boost::uint32_t seek_time = 0;
                    read_demuxer_->seek(seek_time, ec);
                    if (!ec || ec == error::not_support) {
                        read_demuxer_->get_sample(sample, ec);
                        if (!ec) {
                            read_stream_->drop();
                        }
                    }
                    if (ec == error::file_stream_error) {
                        ec = read_stream_->error();
                    }
                } else {
                    ec = read_stream_->error();
                }
            }
            return ec;
        }

        size_t BufferDemuxer::get_media_count(
            boost::system::error_code & ec)
        {
            return read_demuxer_->get_media_count(ec);
        }

        boost::system::error_code BufferDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
        {
            return read_demuxer_->get_media_info(index, info, ec);
        }

        boost::uint32_t BufferDemuxer::get_duration(
            boost::system::error_code & ec)
        {
            return read_demuxer_->get_duration(ec);
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
            case DemuxerType::asf:
                return new AsfDemuxerBase(*buf);
            case DemuxerType::flv:
                return new FlvDemuxerBase(*buf);
            default:
                return new Mp4DemuxerBase(*buf);
            }
        }

        void BufferDemuxer::create_demuxer(
            boost::system::error_code & ec)
        {
            if (!write_demuxer_) {
                write_stream_ = new BytesStream(*buffer_, buffer_->write_segment());
                write_demuxer_ = create_demuxer(
                    (*segments_)[buffer_->write_segment()].demuxer_type,
                    write_stream_);
                write_demuxer_->open(ec);
            }
            if (!read_demuxer_) {
                if (buffer_->read_segment() == buffer_->write_segment()) {
                    read_stream_ = write_stream_;
                    read_demuxer_ = write_demuxer_;
                } else{
                    read_stream_ = new BytesStream(*buffer_, buffer_->read_segment());
                    read_demuxer_ = create_demuxer(
                        (*segments_)[buffer_->read_segment()].demuxer_type, 
                        read_stream_);
                    read_demuxer_->open(ec);
                }
            }
        }

        void BufferDemuxer::create_segment_demuxer(
            size_t segment,
            boost::system::error_code & ec)
        {
            if (segment == buffer_->write_segment()) {
                if (segment != buffer_->read_segment()) {
                    if (read_demuxer_) {
                        delete read_stream_;
                        read_stream_ = NULL;
                        delete read_demuxer_;
                        read_demuxer_ = NULL;
                    }
                    read_stream_ = write_stream_;
                    read_demuxer_ = write_demuxer_;
                }
            } else {
                if (segment == buffer_->read_segment()) {
                    if (write_demuxer_) {
                        delete write_stream_;
                        write_stream_ = NULL;
                        delete write_demuxer_;
                        write_demuxer_ = NULL;
                    }
                    write_stream_ = read_stream_;
                    write_demuxer_ = read_demuxer_;
                } else {
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
                    read_stream_ = write_stream_ = new BytesStream(*buffer_, segment);
                    read_stream_->update_new();
                    read_demuxer_ = write_demuxer_ = create_demuxer(
                        (*segments_)[segment].demuxer_type,
                        read_stream_);
                    read_demuxer_->open(ec);
                }
            }
        }

        void BufferDemuxer::create_read_demuxer(
            boost::system::error_code & ec)
        {
            if (read_demuxer_) {
                delete read_stream_;
                read_stream_ = NULL;
                delete read_demuxer_;
                read_demuxer_ = NULL;
            }
            if (buffer_->read_segment() == buffer_->write_segment()) {
                read_stream_ = write_stream_;
                read_demuxer_ = write_demuxer_;
                read_stream_->update_new();
            } else {
                read_stream_ = new BytesStream(*buffer_, buffer_->read_segment());
                read_stream_->update_new();
                read_demuxer_ = create_demuxer(
                    (*segments_)[buffer_->read_segment()].demuxer_type,
                    read_stream_);
                read_demuxer_->open(ec);
            }
        }

        void BufferDemuxer::create_write_demuxer(
            boost::system::error_code & ec)
        {
            if (write_demuxer_ != read_demuxer_) {
                delete write_stream_;
                write_stream_ = NULL;
                delete write_demuxer_;
                write_demuxer_ = NULL;
            }
            write_stream_ = new BytesStream(*buffer_, buffer_->write_segment());
            write_stream_->update_new();
            write_demuxer_ = create_demuxer(
                (*segments_)[buffer_->write_segment()].demuxer_type, 
                write_stream_);
            write_demuxer_->open(ec);
        }

    } // namespace demux
} // namespace ppbox

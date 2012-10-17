// SegmentDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SegmentDemuxer.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/BytesStream.h"
#include "ppbox/demux/base/DemuxerInfo.h"

#include <ppbox/data/MediaBase.h>
#include <ppbox/data/SegmentSource.h>
#include <ppbox/data/SourceError.h>

using namespace ppbox::avformat;

#include <framework/logger/Logger.h>
#include <framework/timer/Ticker.h>
using namespace framework::logger;

#include <boost/thread/condition_variable.hpp>
#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.SegmentDemuxer", Debug);

namespace ppbox
{
    namespace demux
    {

        SegmentDemuxer::SegmentDemuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::MediaBase & media)
            : io_svc_(io_svc)
            , media_(media)
            , source_(NULL)
            , buffer_(NULL)
            , root_content_(NULL)
            , seek_time_(0)
            , read_demuxer_(NULL)
            , write_demuxer_(NULL)
            , max_demuxer_infos_(5)
            , open_state_(not_open)
        {
            ticker_ = new framework::timer::Ticker(1000);
            events_.reset(new EventQueue);

            root_content_ = new DemuxStrategy(media);
            ppbox::data::SourceBase * source = ppbox::data::SourceBase::create(io_svc, media);
            error_code ec;
            source->set_non_block(true, ec);
            source_ = new ppbox::data::SegmentSource(*root_content_, *source);
            buffer_ = new SegmentBuffer(*source_, 10 * 1024 * 1024, 10240);
        }

        SegmentDemuxer::~SegmentDemuxer()
        {
            boost::system::error_code ec;
            close(ec);
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
            if (ticker_) {
                delete ticker_;
                ticker_ = NULL;
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

        boost::system::error_code SegmentDemuxer::open(
            boost::system::error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(boost::ref(resp));
            resp.wait();
            return ec;
        }

        void SegmentDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            handle_async_open(ec);
        }

        bool SegmentDemuxer::is_open(
            boost::system::error_code & ec)
        {
            if (open_state_ == open_finished) {
                ec.clear();
                return true;
            } else if (open_state_ == not_open) {
                ec = error::not_open;
                return false;
            } else {
                ec = boost::asio::error::would_block;
                return false;
            }
        }

        boost::system::error_code SegmentDemuxer::cancel(
            boost::system::error_code & ec)
        {
            open_state_ = canceling;
            if (source_open == open_state_) {
                media_.cancel();
            } else if (demuxer_open == open_state_) {
                //buffer_->cancel(ec);
            }
            return ec;
        }

        boost::system::error_code SegmentDemuxer::close(
            boost::system::error_code & ec)
        {
            cancel(ec);
            seek_time_ = 0;

            read_demuxer_ = NULL;
            write_demuxer_ = NULL;

            stream_infos_.clear();

            for (boost::uint32_t i = 0; i < demuxer_infos_.size(); ++i) {
                delete demuxer_infos_[i]->demuxer;
                delete demuxer_infos_[i];
            }
            demuxer_infos_.clear();

            open_state_ = not_open;
            return ec;
        }

        void SegmentDemuxer::handle_async_open(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (ec) {
                last_error(ec);
                resp_(ec);
                return;
            }

            switch(open_state_) {
                case not_open:
                    open_state_ = source_open;
                    DemuxStatistic::open_beg();
                    media_.async_open(
                        boost::bind(&SegmentDemuxer::handle_async_open, this, _1));
                    break;
                case source_open:
                    open_state_ = demuxer_open;
                    media_.get_info(media_info_, ec);
                    if (!ec) {
                        // TODO:
                        timestamp_helper_.smoth(media_info_.is_live);
                        buffer_->pause_stream();
                        reset(ec);
                    }
                case demuxer_open:
                    buffer_->pause_stream();
                    if (!ec && !seek(seek_time_, ec)) { // 上面的reset可能已经有错误，所以判断ec
                        size_t stream_count = read_demuxer_->demuxer->get_stream_count(ec);
                        if (!ec) {
                            StreamInfo info;
                            for (boost::uint32_t i = 0; i < stream_count; i++) {
                                read_demuxer_->demuxer->get_stream_info(i, info, ec);
                                stream_infos_.push_back(info);
                            }
                        }
                        open_state_ = open_finished;
                        open_end();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block || (ec == error::file_stream_error 
                        && buffer_->last_error() == boost::asio::error::would_block)) {
                            buffer_->async_prepare_at_least(0, 
                                boost::bind(&SegmentDemuxer::handle_async_open, this, _1));
                    } else {
                        open_state_ = open_finished;
                        open_end();
                        response(ec);
                    }
                    break;
                default:
                    assert(0);
                    break;
            }
        }

        void SegmentDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

        boost::system::error_code SegmentDemuxer::reset(
            boost::system::error_code & ec)
        {
            boost::uint64_t time; 
            if (!root_content_->reset(time, ec)) {
                last_error(ec);
                return ec;
            }
            return seek(time, ec);
        }

        boost::system::error_code SegmentDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            ec.clear();
            SegmentPosition base(buffer_->base_segment());
            SegmentPosition pos(buffer_->read_segment());
            if (&time != &seek_time_) {
                if (!root_content_->time_seek(time, base, pos, ec) 
                    || !buffer_->seek(base, pos, pos.head_size, ec)) {
                        last_error(ec);
                        return ec;
                }
                if (read_demuxer_) {
                    free_demuxer(read_demuxer_, true, ec);
                    read_demuxer_->demuxer->demux_end();
                }
                timestamp_helper_.reset(pos.time_range.big_beg());
                read_demuxer_ = alloc_demuxer(pos, true, ec);
                read_demuxer_->demuxer->demux_begin(timestamp_helper_);
            }
            if (!ec && read_demuxer_->demuxer->is_open(ec)) {
                assert(time >= pos.time_range.big_beg() && pos.time_range.big_end() >= time);
                boost::uint64_t time_t = time - pos.time_range.big_beg();
                pos.byte_range.pos = read_demuxer_->demuxer->seek(time_t, ec);
                if (!ec) {
                    time = pos.time_range.big_beg() + time_t;
                    if (buffer_->seek(base, pos, ec)) {
                        seek_time_ = 0;
                        if (write_demuxer_)
                            free_demuxer(write_demuxer_, false, ec);
                        write_demuxer_ = alloc_demuxer(buffer_->write_segment(), false, ec);
                    }
                }
            }
            if (&time != &seek_time_ && open_state_ == open_finished) {
                DemuxStatistic::seek(!ec, time);
            }
            if (ec) {
                seek_time_ = time; // 用户连续seek，以最后一次为准
                if (ec == error::file_stream_error) {
                    ec = buffer_->last_error();
                }
                last_error(ec);
            }
            buffer_->resume_stream();
            return ec;
        }

        boost::system::error_code SegmentDemuxer::pause(
            boost::system::error_code & ec)
        {
            //source_.pause(ec);
            DemuxStatistic::pause();
            return ec;
        }

        boost::system::error_code SegmentDemuxer::resume(
            boost::system::error_code & ec)
        {
            DemuxStatistic::resume();
            ec.clear();
            return ec;
        }

        boost::system::error_code SegmentDemuxer::get_media_info(
            ppbox::data::MediaInfo & info,
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                media_.get_info(info, ec);
            }
            return ec;
        }

        size_t SegmentDemuxer::get_stream_count(
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                return stream_infos_.size();
            } else {
                return 0;
            }
        }

        boost::system::error_code SegmentDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                if (index < stream_infos_.size()) {
                    info = stream_infos_[index];
                } else {
                    ec = framework::system::logic_error::out_of_range;
                }
            }
            return ec;
        }

        boost::uint64_t SegmentDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            buffer_->prepare_at_least(0, ec);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    return buffer_->read_segment().time_range.big_end(); // 这时间实践上没有意义的
                }
            }

            boost::uint64_t time = 0;
            if (read_demuxer_->demuxer) {
                time = buffer_->read_segment().time_range.big_beg() + read_demuxer_->demuxer->get_cur_time(ec);
                if (ec) {
                    last_error(ec);
                    if (ec == error::file_stream_error) {
                        ec = buffer_->last_error();
                    }
                }
            } else {
                ec.clear();
                time = buffer_->read_segment().time_range.big_end();
            }

            return time;
        }

        boost::uint64_t SegmentDemuxer::get_end_time(
            boost::system::error_code & ec)
        {
            buffer_->prepare_at_least(0, ec);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    return buffer_->write_segment().time_range.big_beg();
                }
            }

            boost::uint64_t time = 0;
            if (write_demuxer_->demuxer) {
                if (write_demuxer_->segment != buffer_->write_segment()) {
                    free_demuxer(write_demuxer_, false, ec);
                    if (buffer_->write_segment().valid()) {
                        write_demuxer_ = alloc_demuxer(buffer_->write_segment(), false, ec);
                    }
                }
                time = buffer_->write_segment().time_range.big_beg() + write_demuxer_->demuxer->get_end_time(ec);
                if (ec) {
                    last_error(ec);
                    if (ec == error::file_stream_error) {
                        ec = buffer_->last_error();
                    }
                }
            } else {
                ec.clear();
                time = buffer_->write_segment().time_range.big_beg();
            }

            return time;
        }

        boost::system::error_code SegmentDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            buffer_->drop(ec);
            buffer_->prepare_at_least(0, ec);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                return ec;
            }

            while (read_demuxer_->demuxer->get_sample(sample, ec)) {
                if (ec != error::file_stream_error && ec != error::no_more_sample) {
                    break;
                }
                if (buffer_->read_segment() != buffer_->write_segment()) {
                    std::cout << "finish segment " << buffer_->read_segment().index << std::endl;
                    buffer_->drop_all(ec);
                    if (buffer_->read_segment().valid()) {
                        read_demuxer_->demuxer->demux_end();
                        free_demuxer(read_demuxer_, true, ec);
                        read_demuxer_ = alloc_demuxer(buffer_->read_segment(), true, ec);
                        read_demuxer_->demuxer->demux_begin(timestamp_helper_);
                        continue;
                    } else {
                        ec = error::no_more_sample;
                    }
                } else {
                    ec = buffer_->last_error();
                    assert(ec);
                    if (ec == ppbox::data::source_error::no_more_segment)
                        ec = error::no_more_sample;
                    if (!ec) {
                        ec = boost::asio::error::would_block;
                    }
                    break;
                }
            }
            if (!ec) {
                play_on(sample.time);
                sample.data.clear();
                for (size_t i = 0; i < sample.blocks.size(); ++i) {
                    buffer_->data(sample.blocks[i].offset, sample.blocks[i].size, sample.data, ec);
                    if (ec) {
                        last_error(ec);
                        break;
                    }
                }
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                last_error(ec);
            }
            return ec;
        }

        boost::system::error_code SegmentDemuxer::get_sample_buffered(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (state_ == buffering && play_position_ > seek_position_ + 30000) {
                boost::system::error_code ec_buf;
                boost::uint64_t time = get_buffer_time(ec, ec_buf);
                if (ec && ec != boost::asio::error::would_block) {
                } else {
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

        boost::uint64_t SegmentDemuxer::get_buffer_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            if (need_seek_time_) {
                seek_position_ = get_cur_time(ec); 
                if (ec) {
                    ec_buf = buffer_->last_error();
                    return 0;
                }
                need_seek_time_ = false;
                play_position_ = seek_position_;
            }
            boost::uint64_t buffer_time = get_end_time(ec);
            buffer_time = buffer_time > play_position_ ? buffer_time - play_position_ : 0;
            //set_buf_time(buffer_time);
            // 直接赋值，减少输出日志，set_buf_time会输出buf_time
            buffer_time_ = buffer_time;
            ec_buf = buffer_->last_error();
            return buffer_time;
        }

        DemuxerInfo * SegmentDemuxer::alloc_demuxer(
            SegmentPosition const & segment, 
            bool is_read, 
            boost::system::error_code & ec)
        {
            // demuxer_info.ref 只在该函数可见
            ec.clear();
            for (size_t i = 0; i < demuxer_infos_.size(); ++i) {
                DemuxerInfo & info = *demuxer_infos_[i];
                if (info.segment.is_same_segment(segment)) {
                    info.attach();
                    if (info.nref == 1) {
                        buffer_->attach_stream(info.stream, is_read);
                    } else if (is_read) {
                        buffer_->change_stream(info.stream, true);
                    }
                    ec.clear();
                    return &info;
                    // TODO: 没有考虑虚拟分段的情况
                }
            }
            if (demuxer_infos_.size() >= max_demuxer_infos_) {
                for (size_t i = 0; i < demuxer_infos_.size(); ++i) {
                    DemuxerInfo & info = *demuxer_infos_[i];
                    if (info.nref == 0) {
                        info.attach();
                        info.segment = segment;
                        boost::system::error_code ec1;
                        info.demuxer->close(ec1);
                        buffer_->attach_stream(info.stream, is_read);
                        info.demuxer->open(ec);
                        return &info;
                        // TODO: 没有考虑格式变化的情况
                    }
                }
            }
            DemuxerInfo * info = new DemuxerInfo(*buffer_);
            info->segment = segment;
            buffer_->attach_stream(info->stream, is_read);
            info->demuxer = Demuxer::create(media_info_.format, info->stream);
            info->demuxer->open(ec);
            demuxer_infos_.push_back(info);
            return info;
        }

        void SegmentDemuxer::free_demuxer(
            DemuxerInfo * info, 
            bool is_read, 
            boost::system::error_code & ec)
        {
            std::vector<DemuxerInfo *>::iterator iter = 
                std::find(demuxer_infos_.begin(), demuxer_infos_.end(), info);
            assert(iter != demuxer_infos_.end());
            if (info->detach()) {
                buffer_->detach_stream(info->stream);
            } else {
                buffer_->change_stream(info->stream, !is_read);
            }
        }

        SegmentDemuxer::post_event_func SegmentDemuxer::get_poster()
        {
            return boost::bind(SegmentDemuxer::post_event, events_, _1);
        }

        void SegmentDemuxer::post_event(
            boost::shared_ptr<EventQueue> const & events, 
            event_func const & event)
        {
            boost::mutex::scoped_lock lock(events->mutex);
            events->events.push_back(event);
        }

        void SegmentDemuxer::handle_events()
        {
            boost::mutex::scoped_lock lock(events_->mutex);
            for (size_t i = 0; i < events_->events.size(); ++i) {
                events_->events[i]();
            }
            events_->events.clear();
        }

        void SegmentDemuxer::on_event(
            util::event::Event const & event)
        {
        }

    } // namespace demux
} // namespace ppbox

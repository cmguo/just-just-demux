// SegmentDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/segment/SegmentDemuxer.h"
#include "ppbox/demux/segment/DemuxerInfo.h"
#include "ppbox/demux/segment/DemuxStrategy.h"
#include "ppbox/demux/basic/JointData.h"

#include <ppbox/data/base/MediaBase.h>
#include <ppbox/data/base/SourceError.h>
#include <ppbox/data/segment/SegmentSource.h>
using namespace ppbox::data;

using namespace ppbox::avformat;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/configure/Config.h>
#include <framework/timer/Ticker.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.SegmentDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        SegmentDemuxer::SegmentDemuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::SegmentMedia & media)
            : Demuxer(io_svc)
            , media_(media)
            , strategy_(NULL)
            , source_(NULL)
            , buffer_(NULL)
            , source_time_out_(5000)
            , buffer_capacity_(10 * 1024 * 1024)
            , buffer_read_size_(10 * 1024)
            , read_demuxer_(NULL)
            , write_demuxer_(NULL)
            , max_demuxer_infos_(5)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(not_open)
        {
            config_.register_module("Source")
                << CONFIG_PARAM_NAME_RDWR("time_out", source_time_out_);

            config_.register_module("Buffer")
                << CONFIG_PARAM_NAME_RDWR("capacity", buffer_capacity_)
                << CONFIG_PARAM_NAME_RDWR("read_size", buffer_read_size_);

            events_.reset(new EventQueue);
        }

        SegmentDemuxer::~SegmentDemuxer()
        {
            boost::system::error_code ec;
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
            if (source_) {
                ppbox::data::UrlSource * source = (ppbox::data::UrlSource *)&source_->source();
                ppbox::data::UrlSource::destroy(source);
                delete source_;
                source_ = NULL;
            }
            if (strategy_) {
                delete strategy_;
                strategy_ = NULL;
            }
        }

        void SegmentDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            handle_async_open(ec);
        }

        bool SegmentDemuxer::is_open(
            boost::system::error_code & ec) const
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

        bool SegmentDemuxer::is_open(
            boost::system::error_code & ec)
        {
            return const_cast<SegmentDemuxer const *>(this)->is_open(ec);
        }

        boost::system::error_code SegmentDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (media_open == open_state_) {
                media_.cancel(ec);
            } else if (demuxer_open == open_state_) {
                source_->cancel(ec);
            }
            return ec;
        }

        boost::system::error_code SegmentDemuxer::close(
            boost::system::error_code & ec)
        {
            DemuxStatistic::close();

            media_.close(ec);

            seek_time_ = 0;
            seek_pending_ = false;
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
                    {
                    strategy_ = new DemuxStrategy(media_);
                    ppbox::data::UrlSource * source = 
                        ppbox::data::UrlSource::create(get_io_service(), media_.get_protocol());
                    if (source == NULL) {
                        source = ppbox::data::UrlSource::create(media_.get_io_service(), media_.segment_protocol());
                    }
                    error_code ec;
                    source->set_non_block(true, ec);
                    source_ = new ppbox::data::SegmentSource(*strategy_, *source);
                    source_->set_time_out(source_time_out_);
                    buffer_ = new ppbox::data::SegmentBuffer(*source_, buffer_capacity_, buffer_read_size_);
                    }
                    open_state_ = media_open;
                    DemuxStatistic::open_beg();
                    media_.async_open(
                        boost::bind(&SegmentDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    open_state_ = demuxer_open;
                    media_.get_info(media_info_, ec);
                    if (!ec) {
                        // TODO:
                        joint_context_.media_flags(media_info_.flags);
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
                        buffer_->set_track_count(stream_count);
                        open_state_ = open_finished;
                        DemuxStatistic::open_end();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block || (ec == error::file_stream_error 
                        && buffer_->last_error() == boost::asio::error::would_block)) {
                            buffer_->async_prepare_some(0, 
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
            if (!strategy_->reset(time, ec)) {
                last_error(ec);
                return ec;
            }
            return seek(time, ec);
        }

        boost::system::error_code SegmentDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            if (&time != &seek_time_ && (!seek_pending_ || time != seek_time_)) {
                LOG_DEBUG("[seek] begin, time: " << time);
                if (write_demuxer_) {
                    free_demuxer(write_demuxer_, false, ec);
                    write_demuxer_ = NULL;
                }
                if (read_demuxer_) {
                    free_demuxer(read_demuxer_, true, ec);
                    write_demuxer_ = NULL;
                }
                SegmentPosition base(buffer_->base_segment());
                SegmentPosition pos(buffer_->read_segment());
                pos.byte_range.pos = 0; // 可能pos不是0
                if (!strategy_->time_seek(time, base, pos, ec) 
                    || !buffer_->seek(base, pos, pos.time_range.pos == 0 ? ppbox::data::invalid_size : pos.head_size, ec)) {
                        last_error(ec);
                        return ec;
                }
                joint_context_.reset(pos.time_range.big_beg());
                read_demuxer_ = alloc_demuxer(pos, true, ec);
                //read_demuxer_->demuxer->joint_begin(joint_context_);
            }
            while (true) {
                read_demuxer_->demuxer->seek(time, ec);
                /* 可能失败原因
                    1. 数据不够 file_stream_error
                        a. 没有下载到足够数据（使用下载错误作为错误码）
                        b. 文件只有一小部分碎片
                    2. 超出实际分段时长 out_of_range
                    3. 其他（不处理）
                        a. 分段文件格式错误 bad_file_format
                 */
                if (!ec) {
                    LOG_DEBUG("[seek] ok, adjust time: " << time);
                    seek_pending_ = false;
                    boost::system::error_code ec1;
                    if (write_demuxer_)
                        free_demuxer(write_demuxer_, false, ec1);
                    buffer_->pause_stream();
                    write_demuxer_ = alloc_demuxer(buffer_->write_segment(), false, ec1);
                } else if (ec == error::file_stream_error) {
                    if (buffer_->read_has_more()) {
                        boost::uint64_t duration = read_demuxer_->demuxer->get_duration(ec);
                        free_demuxer(read_demuxer_, true, ec);
                        boost::uint64_t min_offset = buffer_->read_segment().byte_range.end;
                        if (joint_context_.read_ctx().data()) {
                            min_offset = joint_context_.read_ctx().data()->adjust_offset(buffer_->read_segment().byte_range.end);
                        }
                        buffer_->read_next(duration, min_offset, ec);
                        if (buffer_->read_segment().valid()) {
                            read_demuxer_ = alloc_demuxer(buffer_->read_segment(), true, ec);
                            //read_demuxer_->demuxer->joint_begin(joint_context_);
                            continue;
                        } else {
                            ec = framework::system::logic_error::out_of_range;
                        }
                    } else {
                        ec = buffer_->last_error();
                        assert(ec);
                    }
                } else if (ec == framework::system::logic_error::out_of_range) {
                    LOG_DEBUG("[seek] out_of_range, segment: " << buffer_->read_segment().index);
                    SegmentPosition base(buffer_->base_segment());
                    SegmentPosition pos(buffer_->read_segment());
                    pos.duration = pos.time_range.end = read_demuxer_->demuxer->get_duration(ec);
                    free_demuxer(read_demuxer_, true, ec);
                    if (strategy_->time_seek(time, base, pos, ec) 
                        && buffer_->seek(base, pos, pos.head_size, ec)) {
                            joint_context_.reset(pos.time_range.big_beg());
                            read_demuxer_ = alloc_demuxer(pos, true, ec);
                            //read_demuxer_->demuxer->joint_begin(joint_context_);
                            continue;
                    }
                }
                break;
            }
            seek_time_ = time; // 用户连续seek，以最后一次为准
            if (ec) {
                seek_pending_ = true;
                last_error(ec);
            }
            if (&time != &seek_time_ && open_state_ == open_finished) {
                DemuxStatistic::seek(!ec, time);
            }
            buffer_->resume_stream();
            return ec;
        }

        boost::uint64_t SegmentDemuxer::check_seek(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            if (seek_pending_) {
                seek(seek_time_, ec);
            } else {
                ec.clear();
            }
            return seek_time_;
        }

        boost::system::error_code SegmentDemuxer::pause(
            boost::system::error_code & ec)
        {
            source_->pause();
            DemuxStatistic::pause();
            ec.clear();
            return ec;
        }

        boost::system::error_code SegmentDemuxer::resume(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            DemuxStatistic::resume();
            return ec;
        }

        boost::system::error_code SegmentDemuxer::get_media_info(
            ppbox::data::MediaInfo & info,
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                media_.get_info(info, ec);
            }
            return ec;
        }

        size_t SegmentDemuxer::get_stream_count(
            boost::system::error_code & ec) const
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
            boost::system::error_code & ec) const
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

        bool SegmentDemuxer::fill_data(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            return !ec;
        }

        bool SegmentDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            using ppbox::data::invalid_size;

            if (is_open(ec)) {
                info.byte_range.beg = 0;
                info.byte_range.end = invalid_size;
                info.byte_range.pos = buffer_->in_position();
                info.byte_range.buf = buffer_->out_position();

                info.time_range.beg = 0;
                info.time_range.end = media_info_.duration;
                info.time_range.pos = get_cur_time(ec);
                info.time_range.buf = get_end_time(ec);

                //std::cout << "[SegmentDemuxer::get_stream_status] info.time_range = " 
                //    << info.time_range.pos << " _ " << info.time_range.buf << std::endl;

                if (info.time_range.buf < info.time_range.pos) {
                    info.time_range.buf = info.time_range.pos;
                }

                info.buf_ec = buffer_->last_error();
            }
            return !ec;
        }

        bool SegmentDemuxer::get_data_stat(
            DataStatistic & stat, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                stat = *source_;
            }
            return !ec;
        }

        boost::system::error_code SegmentDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                return ec;
            }
            assert(!seek_pending_);

            if (sample.memory) {
                buffer_->putback(sample.memory);
                sample.memory = NULL;
            }
            sample.data.clear();
            while (read_demuxer_->demuxer->get_sample(sample, ec)) {
                if (ec != error::file_stream_error && ec != error::no_more_sample) {
                    break;
                }
                if (buffer_->read_has_more()) {
                    LOG_DEBUG("[get_sample] finish segment " << buffer_->read_segment().index);
                    boost::uint64_t duration = read_demuxer_->demuxer->get_duration(ec);
                    free_demuxer(read_demuxer_, true, ec);
                    boost::uint64_t min_offset = buffer_->read_segment().byte_range.end;
                    if (joint_context_.read_ctx().data()) {
                        min_offset = joint_context_.read_ctx().data()->adjust_offset(buffer_->read_segment().byte_range.end);
                    }
                    buffer_->read_next(duration, min_offset, ec);
                    if (buffer_->read_segment().valid()) {
                        read_demuxer_ = alloc_demuxer(buffer_->read_segment(), true, ec);
                        //read_demuxer_->demuxer->joint_begin(joint_context_);
                        //if (!ec) { // 已经打开，需要回滚到开始位置
                        //    read_demuxer_->demuxer->reset(ec);
                        //}
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
                DemuxStatistic::play_on(sample.time);
                sample.memory = buffer_->fetch(
                    sample.itrack, 
                    *(std::vector<DataBlock> *)sample.context, 
                    false, 
                    sample.data, 
                    ec);
                assert(!ec);
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                last_error(ec);
            }
            return ec;
        }

        bool SegmentDemuxer::free_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (sample.memory) {
                buffer_->putback(sample.memory);
                sample.memory = NULL;
            }
            ec.clear();
            return true;
        }

        boost::uint64_t SegmentDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                    return buffer_->read_segment().time_range.big_end(); // 这时间实践上没有意义的
                }
            }

            boost::uint64_t time = 0;
            if (read_demuxer_) {
                time = read_demuxer_->demuxer->get_joint_cur_time(ec);
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
            buffer_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                    return buffer_->write_segment().time_range.big_beg();
                }
            }

            boost::uint64_t time = 0;
            buffer_->pause_stream(); // 防止下面代码执行时继续下载，引起 buffer_->write_segment() 变化，导致 write_demuxer_->stream 分段属性错误
            if (write_demuxer_) {
                while (buffer_->write_has_more()) {
                    LOG_DEBUG("[get_end_time] finish segment " << write_demuxer_->segment.index);
                    if (buffer_->write_segment().valid()) {
                        boost::uint64_t duration = write_demuxer_->demuxer->get_duration(ec);
                        free_demuxer(write_demuxer_, false, ec);
                        buffer_->write_next(duration, ec);
                        write_demuxer_ = alloc_demuxer(buffer_->write_segment(), false, ec);
                    } else {
                        break;
                    }
                }
                time = write_demuxer_->demuxer->get_joint_end_time(ec);
                if (ec == error::file_stream_error) {
                    ec.clear();
                }
                if (ec) {
                    last_error(ec);
                }
            } else {
                ec.clear();
                time = buffer_->write_segment().time_range.big_beg();
            }

            return time;
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
                    info.segment = segment;
                    info.attach();
                    if (info.nref == 1) {
                        buffer_->attach_stream(info.stream, is_read);
                    } else if (is_read) {
                        buffer_->change_stream(info.stream, true);
                    }
                    if (is_read) {
                        info.demuxer->joint_begin(joint_context_);
                    } else {
                        info.demuxer->joint_begin2(joint_context_);
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
                        boost::system::error_code ec1;
                        info.demuxer->close(ec1);
                        info.attach();
                        info.segment = segment;
                        buffer_->attach_stream(info.stream, is_read);
                        if (is_read) {
                            info.demuxer->joint_begin(joint_context_);
                        } else {
                            info.demuxer->joint_begin2(joint_context_);
                        }
                        info.demuxer->open(ec);
                        return &info;
                        // TODO: 没有考虑格式变化的情况
                    }
                }
            }
            DemuxerInfo * info = new DemuxerInfo(*buffer_);
            info->segment = segment;
            buffer_->attach_stream(info->stream, is_read);
            info->demuxer = BasicDemuxer::create(media_info_.format, get_io_service(), info->stream);
            if (is_read) {
                info->demuxer->joint_begin(joint_context_);
            } else {
                info->demuxer->joint_begin2(joint_context_);
            }
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
            if (is_read) {
                info->demuxer->joint_end();
            } else {
                info->demuxer->joint_end2();
            }
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

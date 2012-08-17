// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/BytesStream.h"
#include "ppbox/demux/base/Content.h"
#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/base/BufferList.h"

#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"

#include <framework/timer/Ticker.h>
using namespace framework::logger;

#include <boost/thread/condition_variable.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        BufferDemuxer::BufferDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            Content * source)
            : io_svc_(io_svc)
            , root_content_(source)
            , buffer_(NULL)
            , seek_time_(0)
            , segment_time_(0)
            , segment_ustime_(0)
            , video_frame_interval_(0)
            , buffer_size_(buffer_size)
            , prepare_size_(prepare_size)
            , read_demuxer_(NULL)
            , write_demuxer_(NULL)
            , max_demuxer_infos_(5)
            , open_state_(OpenState::not_open)
        {
            ticker_ = new framework::timer::Ticker(1000);
            events_.reset(new EventQueue);
        }

        BufferDemuxer::~BufferDemuxer()
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

        Content * BufferDemuxer::get_contet()
        {
            return root_content_;
        }

        boost::system::error_code BufferDemuxer::open(
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
            open_state_ = OpenState::source_open;
            DemuxerStatistic::open_beg();
            root_content_->get_segment()->async_open(
                common::SegmentBase::OpenMode::fast, 
                boost::bind(&BufferDemuxer::handle_async_open, 
                this, 
                _1));
        }

        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    time = buffer_->read_segment().time_end; // 这时间实践上没有意义的
                }
            } else {
                if (read_demuxer_->demuxer) {
                    time = buffer_->read_segment().time_beg + read_demuxer_->demuxer->get_cur_time(ec);
                } else {
                    ec.clear();
                    time = buffer_->read_segment().time_end;
                }
            }

            DemuxerStatistic::last_error(ec);
            return time;
        }

        boost::uint32_t BufferDemuxer::get_end_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    time = buffer_->write_segment().time_beg;
                }
            } else {
                if (write_demuxer_->demuxer) {
                    write_demuxer_->demuxer->set_stream(*buffer_->get_write_bytesstream());
                    time = buffer_->write_segment().time_beg + write_demuxer_->demuxer->get_end_time(ec);
                    if (ec == error::file_stream_error) {
                        ec = buffer_->last_error();
                    }
                } else {
                    ec.clear();
                    time = buffer_->write_segment().time_beg;
                }
            }
            ec_buf = buffer_->last_error();
            last_error(ec);
            return time;
        }

        boost::system::error_code BufferDemuxer::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            ec.clear();
            SegmentPositionEx position;
            position.segment = 0;
            boost::uint32_t seg_time = 0;
            SegmentPositionEx abs_position;
            root_content_->time_seek(time, abs_position, position, ec);
            if (!ec) {
                if (!buffer_->seek(abs_position, position, 0, ec)) {
                    read_demuxer_ = create_demuxer(
                        read_demuxer_ == NULL ? SegmentPosition() : read_demuxer_->segment, 
                        position, 
                        true, 
                        ec);
                    read_demuxer_->demuxer->set_stream(*buffer_->get_read_bytesstream());
                    more(0);
                    read_demuxer_->demuxer->is_open(ec);
                    if (!ec) {
                        assert((time - position.time_beg) >= 0 && (position.time_end - time) >= 0);
                        boost::uint32_t time_t = time - boost::uint32_t(position.time_beg);
                        boost::uint64_t offset = read_demuxer_->demuxer->seek(time_t, ec);
                        if (!ec) {
                            if (!buffer_->seek(abs_position, position, offset, ec)) {
                                segment_time_ = buffer_->read_segment().time_beg;
                                segment_ustime_ = segment_time_ * 1000;
                                for (size_t i = 0; i < media_time_scales_.size(); i++) {
                                    dts_offset_[i] = 
                                        (boost::uint64_t)segment_time_ * media_time_scales_[i] / 1000;
                                }
                                seek_time_ = 0;
                            }
                        }
                    }
                }
            } else {
                if (ec == source_error::no_more_segment) {
                }
            }
            if (&time != &seek_time_) {
                DemuxerStatistic::seek(ec, time);
            }
            if (ec) {
                seek_time_ = time; // 用户连续seek，以最后一次为准
                if (ec == error::file_stream_error) {
                    ec = boost::asio::error::would_block;
                }
            }
            root_content_->on_error(ec);
            last_error(ec);
            return ec;
        }

        boost::system::error_code BufferDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            more(0);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                return ec;
            }

            boost::system::error_code ec1;
            buffer_->drop(ec1);
            while (read_demuxer_->demuxer->get_sample(sample, ec)) {
                if (ec == ppbox::demux::error::file_stream_error
                    || ec == error::no_more_sample) {
                    if (buffer_->read_segment() != buffer_->write_segment()) {
                        std::cout << "finish segment " << buffer_->read_segment().segment << std::endl;
                        boost::uint32_t cur_time = read_demuxer_->demuxer->get_cur_time(ec);
                        // EVENT_SEG_DEMUXER_STOP event
                        SegmentPositionEx position;
                        position.segment = buffer_->read_segment().segment;
                        position.time_state = SegmentPositionEx::is_valid;
                        position.time_beg = 0;
                        position.time_end = cur_time;
                        position.size_beg = 0;
                        position.size_end = boost::uint64_t(-1);
                        position.shard_beg = 0;
                        position.shard_end = position.size_end;
                        Event evt(Event::EVENT_SEG_DEMUXER_STOP, position, boost::system::error_code());
                        root_content_->on_event(evt);

                        buffer_->drop_all(ec1);
                        if (buffer_->read_segment().source) {
                            size_t insert_time = read_demuxer_->segment.source->insert_time();
                            read_demuxer_ = create_demuxer(
                                read_demuxer_ == NULL ? SegmentPosition() : read_demuxer_->segment, 
                                const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                                true, 
                                ec);
                            read_demuxer_->demuxer->set_stream(*buffer_->get_read_bytesstream());
                            read_demuxer_->is_read_stream = true;
                            boost::uint64_t segment_time = buffer_->read_segment().time_beg;
                            if (buffer_->read_segment().time_state != SegmentPositionEx::is_valid) {
                                std::cout << "segment time: " << segment_time_ << " cur time: " << cur_time << std::endl;
                                segment_time_ += cur_time;
                                segment_time_ += video_frame_interval_;
                            } else {
                                segment_time_ = segment_time;
                            }
                            segment_ustime_ = (boost::uint64_t)segment_time_ * 1000;
                            for (size_t i = 0; i < media_time_scales_.size(); i++) {
                                dts_offset_[i] = 
                                    (boost::uint64_t)segment_time_ * media_time_scales_[i] / 1000;
                            }
                            continue;
                        } else {
                            ec = error::no_more_sample;
                        }
                    }
                    if (ec == error::file_stream_error) {
                        if (buffer_->last_error()) {
                            ec = buffer_->last_error();
                        } else {
                            ec = boost::asio::error::would_block;
                        }
                    }
                }
                break;
            }
            if (!ec) {
                sample.time += segment_time_;
                sample.ustime += segment_ustime_;
                if (sample.itrack < dts_offset_.size()) {
                    sample.dts += dts_offset_[sample.itrack];
                }
                play_on(sample.time);
                sample.data.clear();
                boost::uint32_t cur_time = read_demuxer_->demuxer->get_cur_time(ec);
                for (size_t i = 0; i < sample.blocks.size(); ++i) {
                    buffer_->peek(sample.blocks[i].offset, sample.blocks[i].size, sample.data, ec);
                    if (ec) {
                        DemuxerStatistic::last_error(ec);
                        break;
                    }
                }
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxerStatistic::block_on();
                }
                last_error(ec);
            }
            return ec;
        }

        size_t BufferDemuxer::get_media_count(
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                return stream_infos_.size();
            } else {
                return 0;
            }
        }

        boost::system::error_code BufferDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
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

        boost::system::error_code BufferDemuxer::get_duration(
            ppbox::common::DurationInfo & info,
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                root_content_->get_segment()->get_duration(info, ec); // ms
            }
            return ec;
        }

        boost::system::error_code BufferDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (OpenState::source_open == open_state_) {
                root_content_->get_segment()->cancel();
            } else if (OpenState::demuxer_open == open_state_) {
                root_content_->get_segment()->close(); // source opened
                buffer_->cancel(ec);
            }
            open_state_ = OpenState::cancel;
            return ec;
        }

        boost::system::error_code BufferDemuxer::close(
            boost::system::error_code & ec)
        {
            cancel(ec);
            seek_time_ = 0;
            segment_time_ = 0;
            segment_ustime_ = 0;

            read_demuxer_ = NULL;
            write_demuxer_ = NULL;

            media_time_scales_.clear();
            dts_offset_.clear();
            stream_infos_.clear();

            for (boost::uint32_t i = 0; i < demuxer_infos_.size(); ++i) {
                delete demuxer_infos_[i]->demuxer;
                delete demuxer_infos_[i];
            }
            demuxer_infos_.clear();

            open_state_ = OpenState::not_open;
            return ec;
        }

        void BufferDemuxer::handle_async_open(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (ec) {
                last_error(ec);
                resp_(ec);
            } else {
                boost::system::error_code ec1;
                boost::system::error_code ec2;
                boost::uint32_t stream_count = 0;
                switch(open_state_) {
                    case OpenState::source_open:
                        open_state_ = OpenState::demuxer_open;
                        buffer_ = new BufferList(buffer_size_, prepare_size_, root_content_, this);
                        read_demuxer_ = create_demuxer(
                            read_demuxer_ == NULL ? SegmentPosition() : read_demuxer_->segment,
                            buffer_->read_segment(), 
                            true, 
                            ec1);
                        // write_demuxer都是在on_event事件中创建
                        buffer_->async_prepare_at_least(0, boost::bind(&BufferDemuxer::handle_async_open, this, _1));
                        break;
                    case OpenState::demuxer_open:
                        read_demuxer_->demuxer->is_open(ec2);
                        if (ec2) {
                            buffer_->async_prepare_at_least(0, boost::bind(&BufferDemuxer::handle_async_open, this, _1));
                        } else {
                            root_content_->set_buffer_list(buffer_);
                            stream_count = read_demuxer_->demuxer->get_media_count(ec2);
                            if (!ec2) {
                                MediaInfo info;
                                for (boost::uint32_t i = 0; i < stream_count; i++) {
                                    read_demuxer_->demuxer->get_media_info(i, info, ec2);
                                    if (info.type == MEDIA_TYPE_VIDE && info.video_format.frame_rate) {
                                        video_frame_interval_ = 1000 / info.video_format.frame_rate;
                                    }
                                    stream_infos_.push_back(info);
                                    media_time_scales_.push_back(stream_infos_[i].time_scale);
                                    dts_offset_.push_back(stream_infos_[i].time_scale);
                                    segment_time_ = buffer_->read_segment().time_beg;
                                    segment_ustime_ = segment_time_ * 1000;
                                    for (size_t i = 0; i < media_time_scales_.size(); i++) {
                                        dts_offset_[i] = 
                                            (boost::uint64_t)segment_time_ * media_time_scales_[i] / 1000;
                                    }
                                }
                                if (info.video_format.frame_rate == 0) {
                                    video_frame_interval_ = 40;
                                }
                                open_state_ = OpenState::open_finished;
                                open_end();
                            }
                            response(ec);
                        }
                        break;
                    default:
                        assert(0);
                        break;
                }
            }
        }

        void BufferDemuxer::handle_segment_open_event(
            boost::uint64_t duration,
            boost::uint64_t filesize,
            error_code const & ec)
        {
            SegmentPositionEx position;
            position.segment = open_segment_.segment;
            position.time_beg = 0;
            position.time_end = duration;
            position.size_beg = 0;
            position.size_end = filesize;
            position.shard_beg = 0;
            position.shard_end = position.size_end;
            Event evt(Event::EVENT_SEG_DEMUXER_OPEN, position, boost::system::error_code());
            root_content_->on_event(evt);
        } 

        bool BufferDemuxer::is_open(boost::system::error_code & ec)
        {
            if (open_state_ == OpenState::open_finished) {
                ec.clear();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        void BufferDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

         DemuxerInfo * BufferDemuxer::create_demuxer(
             SegmentPosition const & unref, 
             SegmentPositionEx const & segment, 
             bool using_read_stream,
             boost::system::error_code & ec)
         {
            // demuxer_info.ref 只在该函数可见
            ec.clear();
            bool find = false;
            DemuxerInfo * ref = NULL;
            for (boost::uint32_t i = 0; i < demuxer_infos_.size(); ++i) {
                if (demuxer_infos_[i]->segment == segment) {
                    demuxer_infos_[i]->ref++;
                    ref = demuxer_infos_[i];
                    find = true;
                }
                demuxer_infos_[i]->segment == unref 
                    && demuxer_infos_[i]->ref != 0 
                    && demuxer_infos_[i]->ref--;
            }

            if (!find) {
                DemuxerInfo * info = new DemuxerInfo;
                info->demuxer = create_demuxer_base(root_content_->demuxer_type(), 
                    using_read_stream ? *buffer_->get_read_bytesstream() : *buffer_->get_write_bytesstream());
                info->segment = segment;
                info->ref = 1;
                info->is_read_stream = using_read_stream;
                demuxer_infos_.push_back(info);
                ref = info;
                open_segment_ = ref->segment;
                ref->demuxer->open(ec, boost::bind(&BufferDemuxer::handle_segment_open_event, this, _1, _2, _3));
                if (demuxer_infos_.size() >= max_demuxer_infos_) {
                    for (std::vector<DemuxerInfo*>::iterator iter = demuxer_infos_.begin(); iter != demuxer_infos_.end(); ++iter) {
                        if ((*iter)->ref == 0) {
                            delete (*iter)->demuxer;
                            delete *iter;
                            demuxer_infos_.erase(iter);
                            break;
                        }
                    }
                }
                assert(demuxer_infos_.size() < max_demuxer_infos_);
            }
            return ref;
        }

        BufferDemuxer::post_event_func BufferDemuxer::get_poster()
        {
            return boost::bind(BufferDemuxer::post_event, events_, _1);
        }

        void BufferDemuxer::post_event(
            boost::shared_ptr<EventQueue> const & events, 
            event_func const & event)
        {
            boost::mutex::scoped_lock lock(events->mutex);
            events->events.push_back(event);
        }

        void BufferDemuxer::handle_events()
        {
            boost::mutex::scoped_lock lock(events_->mutex);
            for (size_t i = 0; i < events_->events.size(); ++i) {
                events_->events[i]();
            }
            events_->events.clear();
        }

        void BufferDemuxer::tick_on()
        {
            if (ticker_->check()) {
                process_insert_media();
                update_stat();
            }
        }

        void BufferDemuxer::update_stat()
        {
            boost::system::error_code ec;
            boost::system::error_code ec_buf;
            boost::uint32_t buffer_time = get_buffer_time(ec, ec_buf);
            buf_time(buffer_time);
        }

        void BufferDemuxer::on_extern_error(
            boost::system::error_code const & ec)
        {
            extern_error_ = ec;
        }

        void BufferDemuxer::on_event(
            Event const & event)
        {
            boost::system::error_code ec;
            if (Event::EVENT_SEG_DL_OPEN == event.evt_type ) {
                write_demuxer_ = create_demuxer(
                    write_demuxer_ == NULL ? SegmentPosition() : write_demuxer_->segment,
                    event.seg_info, 
                    false, 
                    ec);
            }
        }

        void BufferDemuxer::on_error(
            boost::system::error_code & ec)
        {
            if (ec == source_error::at_end_point) {
                if (seek_time_) {
                    boost::system::error_code ec1;
                    seek(seek_time_, ec1);
                }
                SegmentPositionEx old_seg = buffer_->write_segment();
                Content * source = (Content *)old_seg.next_child;
                if (source) {// 说明本段有分段插入，source指向插入分段
                    boost::system::error_code ec2;
                    int num = 0;
                    while (source && source->insert_segment() == write_demuxer_->segment.segment) {
                        num++;
                        if (num == 1) {
                            source->insert_demuxer() = *write_demuxer_;
                        }
                        boost::uint32_t delta;
                        boost::uint32_t time = source->insert_time();// 获取插入的时间点
                        boost::uint64_t offset = write_demuxer_->demuxer->get_offset(time, delta, ec2);  // 根据时间获取插入位置
                        if (!ec2) {
                            source->update_insert(buffer_->write_segment(), time, offset, delta);       //
                            buffer_->insert_source(offset, source, source->tree_size() + delta, ec2);   // 真插入
                        }
                        source = source->next_sibling();
                    }
                    // 可能next_child变了
                    read_demuxer_->segment = buffer_->read_segment();
                    write_demuxer_ = create_demuxer(
                        write_demuxer_ == NULL ? SegmentPosition() : write_demuxer_->segment, 
                        buffer_->write_segment(), 
                        false, 
                        ec2);
                    boost::uint64_t seek_end = old_seg.source->next_end(old_seg);
                    if (seek_end != (boost::uint64_t)-1) {
                        //buffer_->seek(
                        //    const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                        //    buffer_->read_offset() - buffer_->read_segment().size_beg, 
                        //    seek_end + old_seg.source->segment_head_size(old_seg.segment) - buffer_->read_segment().size_beg, 
                        //    ec2);
                    }
                }
                ec.clear();
            }
            DemuxerStatistic::last_error(ec);
        }

        void BufferDemuxer::more(boost::uint32_t size)
        {
            boost::system::error_code ec;
            buffer_->prepare_at_least(size, ec);
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

        boost::system::error_code BufferDemuxer::set_non_block(
            bool non_block, 
            boost::system::error_code & ec)
        {
            return root_content_->get_source()->set_non_block(non_block, ec);
        }

        boost::system::error_code BufferDemuxer::set_time_out(
            boost::uint32_t time_out, 
            boost::system::error_code & ec)
        {
            return root_content_->get_source()->set_time_out(time_out, ec);
            return ec;
        }

        boost::system::error_code BufferDemuxer::pause(
            boost::system::error_code & ec)
        {
            return ec = error::not_support;
        }

        boost::system::error_code BufferDemuxer::resume(
            boost::system::error_code & ec)
        {
            return ec = error::not_support;
        }

    } // namespace demux
} // namespace ppbox

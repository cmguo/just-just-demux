// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/BytesStream.h"
#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/base/BufferList.h"

#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"

#include <framework/timer/Ticker.h>
using namespace framework::logger;
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
            SourceBase * source)
            : io_svc_(io_svc)
            , root_source_(source)
            , buffer_(NULL)
            , seek_time_(0)
            , segment_time_(0)
            , segment_ustime_(0)
            , buffer_size_(buffer_size)
            , prepare_size_(prepare_size)
            , max_demuxer_infos_(5)
            , open_state_(OpenState::not_open)
        {
            ticker_ = new framework::timer::Ticker(1000);
            events_.reset(new EventQueue);
        }

        BufferDemuxer::~BufferDemuxer()
        {
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

        boost::system::error_code BufferDemuxer::open (
            std::string const & name, 
            boost::system::error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(name, boost::ref(resp));
            resp.wait();
            return ec;
        }

        void BufferDemuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            open_state_ = OpenState::source_open;
            DemuxerStatistic::open_beg();
            root_source_->set_url(name);
            root_source_->async_open(boost::bind(&BufferDemuxer::handle_async_open, this, _1));
        }

        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            //if (!events_->events.empty()) {
            //    handle_events();
            //}
            more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    time = buffer_->read_segment().time_end; // 这时间实践上没有意义的
                }
            } else {
                if (read_demuxer_.demuxer) {
                    time = buffer_->read_segment().time_beg + read_demuxer_.demuxer->get_cur_time(ec);
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
            //if (!events_->events.empty()) {
            //    handle_events();
            //}
            //tick_on();
            more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    time = buffer_->write_segment().time_beg;
                }
            } else {
                if (write_demuxer_.demuxer) {
                    write_demuxer_.demuxer->set_stream(*buffer_->get_write_bytesstream());
                    time = buffer_->write_segment().time_beg + write_demuxer_.demuxer->get_end_time(ec);
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
            //ec.clear();
            //SegmentPositionEx position;
            //position.segment = 0;
            //boost::uint32_t seg_time = 0;
            //root_source_->time_seek(time, position, ec);
            //if (ec == source_error::no_more_segment) { // 找不到对应的段
            //}
            //if (!ec) {
            //    if (!buffer_->seek(position, 0, ec)) {
            //        create_demuxer(position, read_demuxer_, ec);
            //        if (!ec) {
            //            create_demuxer(position, write_demuxer_, ec);
            //            assert((time - position.time_beg) >= 0 && (position.time_end - time) >= 0);
            //            boost::uint32_t time_t = time - boost::uint32_t(position.time_beg);
            //            boost::uint64_t offset = read_demuxer_.demuxer->seek(time_t, ec);
            //            more(0);
            //            if (!ec) {
            //                if (!buffer_->seek(position, offset, ec)) {
            //                    seek_time_ = 0;
            //                }
            //            }
            //        }
            //    }
            //}
            //if (&time != &seek_time_) {
            //    DemuxerStatistic::seek(ec, time);
            //}
            //if (ec) {
            //    seek_time_ = time; // 用户连续seek，以最后一次为准
            //    if (ec == error::file_stream_error) {
            //        ec = boost::asio::error::would_block;
            //    }
            //}
            //root_source_->on_error(ec);
            //last_error(ec);
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
            while (read_demuxer_.demuxer->get_sample(sample, ec)) {
                if (ec == ppbox::demux::error::file_stream_error
                    || ec == error::no_more_sample) {
                    if (buffer_->read_segment() != buffer_->write_segment()) {
                        std::cout << "finish segment " << buffer_->read_segment().segment << std::endl;
                        boost::uint32_t cur_time = read_demuxer_.demuxer->get_cur_time(ec);
                        // EVENT_SEG_DEMUXER_STOP event
                        SegmentPositionEx position;
                        position.segment = buffer_->read_segment().segment;
                        position.time_beg = 0;
                        position.time_end = cur_time;
                        position.size_beg = 0;
                        position.size_end = boost::uint32_t(-1);
                        position.shard_beg = 0;
                        position.shard_end = position.size_end;
                        Event evt(Event::EVENT_SEG_DEMUXER_STOP, position, boost::system::error_code());
                        root_source_->on_event(evt);

                        buffer_->drop_all(ec1);
                        if (buffer_->read_segment().source) {
                            size_t insert_time = read_demuxer_.segment.source->insert_time();
                            change_source(const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                                read_demuxer_, true, ec);
                            read_demuxer_.demuxer->set_stream(*buffer_->get_read_bytesstream());
                            boost::uint64_t segment_time = buffer_->read_segment().time_beg;
                            if (segment_time == boost::uint64_t(-1)) {
                                segment_time_ += cur_time;
                            } else {
                                segment_time_ = segment_time;
                            }
                            segment_time_ -= insert_time;
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
                        ec = buffer_->last_error();
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
            DurationInfo & info,
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                root_source_->get_duration(info, ec); // ms
            }
            return ec;
        }

        boost::system::error_code BufferDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (OpenState::source_open == open_state_) {
                root_source_->cancel(ec);
            } else if (OpenState::demuxer_open == open_state_) {
                root_source_->close(ec); // source opened
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

            read_demuxer_.demuxer = NULL;
            write_demuxer_.demuxer = NULL;

            media_time_scales_.clear();
            dts_offset_.clear();
            stream_infos_.clear();

            open_state_ = OpenState::not_open;
            return ec;
        }

        boost::system::error_code BufferDemuxer::insert_source(
            boost::uint32_t & time, 
            SourceBase * source, 
            boost::system::error_code & ec)
        {
            SegmentPositionEx position;
            root_source_->time_insert(time, source, position, ec);
            if (!ec) {
                if (position.segment >= buffer_->read_segment().segment
                    && position.segment <= buffer_->write_segment().segment) { // 插入分段在读写分段之间（即当前播放分段，则马上处理）
                        DemuxerInfo demuxer;
                        create_demuxer(position, demuxer, ec);
                        if (!source->insert_demuxer().demuxer) {
                            source->insert_demuxer() = demuxer;
                        }
                        boost::uint32_t delta;
                        boost::uint64_t offset = demuxer.demuxer->get_offset(time, delta, ec);
                        if (!ec) {
                            source->update_insert(position, time, offset, delta);// 获取插入的具体位置，进行更新
                            buffer_->insert_source(offset, source, source->tree_size() + delta, ec);
                            //SegmentPositionEx read_seg = buffer_->read_segment();
                            //SegmentPositionEx write_seg = buffer_->write_segment();
                            //read_demuxer_.stream->unchecked_update( read_seg );
                            //write_demuxer_.stream->unchecked_update( write_seg );
                        }
                } else { // 不是操作当前分段，设定截止点为当前读位置--->被插入分段的头部结束点
                    buffer_->seek(
                        const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                        buffer_->read_offset() - buffer_->read_segment().size_beg, 
                        position.source->segment_head_size(position.segment) + position.size_beg 
                            - buffer_->read_segment().size_beg, ec);
                }
            }
            return ec;
        }

        boost::system::error_code BufferDemuxer::remove_source(
            SourceBase * source, 
            boost::system::error_code & ec)
        {
            return ec;
        }

        void BufferDemuxer::change_source(
            SegmentPositionEx & new_segment, 
            DemuxerInfo & demuxer,
            bool is_seek, 
            boost::system::error_code & ec)
        {
            SourceBase * old_source = demuxer.segment.source;
            if (old_source != new_segment.source) {
                if (old_source->parent() == new_segment.source) {  //子切父
                    if (old_source->insert_demuxer().demuxer) {
                        //reload_demuxer(old_source->insert_demuxer().demuxer, new_segment, demuxer, old_source->insert_input_time(), is_seek, ec);
                    } else {
                        SourceBase * prev_sibling = old_source->prev_sibling();
                        bool flag = false;
                        while (prev_sibling ) {
                            if (prev_sibling->insert_demuxer().demuxer
                                && prev_sibling->insert_segment() == old_source->insert_segment()) {
                                    flag = true;
                                    //reload_demuxer(prev_sibling->insert_demuxer().demuxer, new_segment, demuxer, old_source->insert_input_time(), is_seek, ec);
                                    break;
                            }
                            prev_sibling = prev_sibling->prev_sibling();
                        }
                        assert(flag);
                    }
                } else {  //父切子
                    new_segment.source->insert_demuxer() = demuxer;
                    create_demuxer(new_segment, demuxer, ec);
                }
            } else {
                create_demuxer(new_segment, demuxer, ec);
            }
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
                        buffer_ = new BufferList(buffer_size_, prepare_size_, root_source_, this);
                        create_demuxer(buffer_->read_segment(), read_demuxer_, ec1);
                        create_demuxer(buffer_->write_segment(), write_demuxer_, ec1);
                        buffer_->async_prepare_at_least(0, boost::bind(&BufferDemuxer::handle_async_open, this, _1));
                        break;
                    case OpenState::demuxer_open:
                        read_demuxer_.demuxer->is_open(ec2);
                        if (ec2) {
                            buffer_->async_prepare_at_least(0, boost::bind(&BufferDemuxer::handle_async_open, this, _1));
                        } else {
                            root_source_->set_buffer_list(buffer_);
                            stream_count = read_demuxer_.demuxer->get_media_count(ec2);
                            if (!ec2) {
                                MediaInfo info;
                                for (boost::uint32_t i = 0; i < stream_count; i++) {
                                    read_demuxer_.demuxer->get_media_info(i, info, ec2);
                                    stream_infos_.push_back(info);
                                    media_time_scales_.push_back(stream_infos_[i].time_scale);
                                    dts_offset_.push_back(stream_infos_[i].time_scale);
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
            position.segment = write_demuxer_.segment.segment;
            position.time_beg = 0;
            position.time_end = duration;
            position.size_beg = 0;
            position.size_end = filesize;
            position.shard_beg = 0;
            position.shard_end = position.size_end;
            Event evt(Event::EVENT_SEG_DEMUXER_OPEN, position, boost::system::error_code());
            root_source_->on_event(evt);
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

        void BufferDemuxer::create_demuxer(
            SegmentPositionEx const & segment, 
            DemuxerInfo & demuxer_info, 
            boost::system::error_code & ec)
        {
            // demuxer_info.ref 只在该函数可见
            ec.clear();
            bool find = false;
            for (boost::uint32_t i = 0; i < demuxer_infos_.size(); ++i) {
                if (demuxer_infos_[i].segment == segment) {
                    demuxer_info.segment = demuxer_infos_[i].segment;
                    demuxer_info.demuxer = demuxer_infos_[i].demuxer;
                    demuxer_infos_[i].ref++;
                    find = true;
                }
                demuxer_infos_[i].segment == demuxer_info.segment 
                    && demuxer_infos_[i].ref != 0 
                    && demuxer_infos_[i].ref--;
            }

            if (!find) {
                demuxer_info.demuxer = create_demuxer_base(root_source_->demuxer_type(), *buffer_->get_read_bytesstream());
                demuxer_info.segment = segment;
                demuxer_info.ref = 1;
                demuxer_info.demuxer->open(ec, boost::bind(&BufferDemuxer::handle_segment_open_event, this, _1, _2, _3));
                if (demuxer_infos_.size() >= max_demuxer_infos_) {
                    for (std::vector<DemuxerInfo>::const_iterator iter = demuxer_infos_.begin(); iter != demuxer_infos_.end(); ++iter) {
                        if ((*iter).ref == 0) {
                            delete (*iter).demuxer;
                            demuxer_infos_.erase(iter);
                            break;
                        }
                    }
                }
                assert(demuxer_infos_.size() < max_demuxer_infos_);
                demuxer_infos_.push_back(demuxer_info);
            }
            
            //if (segment == read_demuxer_.segment) {
            //    demuxer_info.demuxer = read_demuxer_.demuxer;
            //    demuxer_info.segment = read_demuxer_.segment;
            //} else if (segment == write_demuxer_.segment) {
            //    demuxer_info.demuxer = write_demuxer_.demuxer;
            //    demuxer_info.segment = write_demuxer_.segment;
            //} else {
            //    demuxer_info.segment = segment;
            //    // demux
            //    //demuxer.demuxer.reset(
            //    //    new Mp4DemuxerBase(demuxer.is_read_stream ? *buffer_->get_read_bytesstream() : *buffer_->get_write_bytesstream()));
            //    demuxer_info.demuxer = new FlvDemuxerBase(buffer_->get_read_bytesstream());
            //    demuxer_info.demuxer->open(ec, boost::bind(&BufferDemuxer::handle_segment_open_event, this, _1, _2, _3));
            //}
        }

        //void BufferDemuxer::reload_demuxer(
        //    DemuxerPointer & demuxer, 
        //    SegmentPositionEx & segment, 
        //    DemuxerInfo & demuxer_info, 
        //    boost::uint32_t time, 
        //    bool is_seek, 
        //    boost::system::error_code & ec)
        //{
        //    if (segment == read_demuxer_.segment) {
        //        //demuxer_info.stream = read_demuxer_.stream;
        //    } else if (segment == write_demuxer_.segment) {
        //        //demuxer_info.stream = write_demuxer_.stream;
        //    } else {
        //        BytesStream * stream = new BytesStream(*buffer_, const_cast<SegmentPositionEx &>(segment));
        //        stream->update_new(segment);
        //        //demuxer_info.stream.reset(stream);
        //    }
        //    //demuxer_info.demuxer.reset(demuxer->clone(* demuxer_info.stream));
        //    demuxer_info.segment = segment;
        //    if (is_seek) {
        //        demuxer_info.demuxer->seek(time, ec);
        //    }
        //}

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
                create_demuxer(event.seg_info, write_demuxer_, ec);
            }
        }

        void BufferDemuxer::on_error(
            boost::system::error_code & ec)
        {
            if (ec == source_error::at_end_point) {
                //write_demuxer_.stream->update_new(
                //    buffer_->write_segment());
                if (seek_time_) {
                    boost::system::error_code ec1;
                    seek(seek_time_, ec1);
                }
                SegmentPositionEx old_seg = buffer_->write_segment();
                SourceBase * source = (SourceBase *)old_seg.next_child;
                if (source) {// 说明本段有分段插入，source指向插入分段
                    boost::system::error_code ec2;
                    int num = 0;
                    while (source && source->insert_segment() == write_demuxer_.segment.segment) {
                        num++;
                        if (num == 1) {
                            source->insert_demuxer() = write_demuxer_;
                        }
                        boost::uint32_t delta;
                        boost::uint32_t time = source->insert_time();// 获取插入的时间点
                        boost::uint64_t offset = write_demuxer_.demuxer->get_offset(time, delta, ec2);  // 根据时间获取插入位置
                        if (!ec2) {
                            source->update_insert(buffer_->write_segment(), time, offset, delta);       //
                            buffer_->insert_source(offset, source, source->tree_size() + delta, ec2);   // 真插入
                        }
                        source = source->next_sibling();
                    }
                    // 可能next_child变了
                    read_demuxer_.segment = buffer_->read_segment();
                    create_demuxer(buffer_->write_segment(), write_demuxer_, ec2);
                    boost::uint64_t seek_end = old_seg.source->next_end(old_seg);
                    if (seek_end != (boost::uint64_t)-1) {
                        buffer_->seek(
                            const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                            buffer_->read_offset() - buffer_->read_segment().size_beg, 
                            seek_end + old_seg.source->segment_head_size(old_seg.segment) - buffer_->read_segment().size_beg, 
                            ec2);
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
            return root_source_->set_non_block(non_block, ec);
        }

        boost::system::error_code BufferDemuxer::set_time_out(
            boost::uint32_t time_out, 
            boost::system::error_code & ec)
        {
            return root_source_->set_time_out(time_out, ec);
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

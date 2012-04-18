// BufferDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/BytesStream.h"
#include "ppbox/demux/base/DemuxerType.h"
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
            SourceBase * source)
            : io_svc_(io_svc)
            , root_source_(source)
            , buffer_(new BufferList(buffer_size, prepare_size, (SourceBase *)source, this))
            , seek_time_(0)
            , segment_time_(0)
            , segment_ustime_(0)
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
            buffer_->source_init();
            create_demuxer(buffer_->read_segment(), read_demuxer_, ec);
            create_demuxer(buffer_->write_segment(), write_demuxer_, ec);
            handle_async(boost::system::error_code());
        }

        // 获取读位置的时间
        boost::uint32_t BufferDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            if (!events_->events.empty()) { // 双保险
                handle_events();
            }
            boost::uint32_t time = 0;

            //if (read_demuxer_.demuxer) {
            //    time = buffer_->read_segment().time_beg + read_demuxer_.demuxer->get_cur_time(ec);
            //} else { // 可能已经播放结束了
            //    ec.clear();
            //    time = buffer_->read_segment().time_end;
            //}

            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    time = buffer_->read_segment().time_end;
                }
            } else {
                if (read_demuxer_.demuxer) {
                    time = buffer_->read_segment().time_beg + read_demuxer_.demuxer->get_cur_time(ec);
                } else { // 可能已经播放结束了
                    ec.clear();
                    time = buffer_->read_segment().time_end;
                }
            }

            last_error(ec);
            return time;
        }

        boost::uint32_t BufferDemuxer::get_end_time(
            boost::system::error_code & ec, 
            boost::system::error_code & ec_buf)
        {
            if (!events_->events.empty()) { // 双保险
                handle_events();
            }
            tick_on();
            StreamPointer write_point = write_demuxer_.stream;
            write_point->write_more(0);
            boost::uint32_t time = 0;
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    //time = buffer_->write_segment().time_end;
                    time = buffer_->write_segment().time_beg;
                }
            } else {
                if (write_demuxer_.demuxer) {
                    time = buffer_->write_segment().time_beg + write_demuxer_.demuxer->get_end_time(ec);
                    if (ec == error::file_stream_error) {
                        ec = write_demuxer_.stream->error();
                    }
                } else { // 可能已经下载结束了
                    ec.clear();
                    // 下载完最后一个分段时，Bufferlist保证写指针信息不被清空
                    //time = buffer_->write_segment().time_end;
                    time = buffer_->write_segment().time_beg;
                }
            }
            ec_buf = write_demuxer_.stream->error();
            last_error(ec);
            return time;
        }

        boost::system::error_code BufferDemuxer::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            // 如果找不到对应的分段，错误码就是source_error::no_more_segment
            //ec = source_error::no_more_segment;
            //ec.clear();
            SegmentPositionEx position;
            boost::uint32_t seg_time = 0;
            root_source_->time_seek(time, position, ec);
            if (!ec) {
                SourceBase * first_src = NULL;
                SourceBase * near_src = NULL;
                bool is_prev = false;
                SourceBase * item = (SourceBase *)position.source->first_child();
                while (item) { // 处理插入分段
                    if (item->insert_segment() == position.segment) {
                        if (!first_src) {
                            near_src = first_src = item;
                        }
                        boost::uint64_t total_time = root_source_->total_time_before(item);
                        if (time > total_time) {
                            near_src = item;
                        }
                    }
                    item = (SourceBase *)item->next_sibling();
                    if (item && item->insert_segment() > position.segment) {
                        break;
                    }
                }
                if (first_src) {
                    boost::uint64_t total_time = root_source_->total_time_before(first_src);
                    if (time > total_time) {
                        is_prev = true;
                        read_demuxer_ = first_src->insert_demuxer();
                    } else {
                        create_demuxer(position, read_demuxer_, ec);
                    }
                } else {
                    create_demuxer(position, read_demuxer_, ec);
                }
                seg_time = time - position.time_beg; // 相对于开头的时间
                boost::uint64_t offset = read_demuxer_.demuxer->seek(seg_time, ec);// 计算分段位置
                segment_time_ = position.time_beg;// 当前分段开始时间（绝对位置）
                segment_ustime_ = segment_time_ * 1000;
                for (size_t i = 0; i < media_time_scales_.size(); i++) {
                    dts_offset_[i] = 
                        segment_time_ * media_time_scales_[i] / 1000;
                }
                if (!ec) {
                    seek_time_ = 0;
                    if (is_prev) {
                        reload_demuxer(read_demuxer_.demuxer, position, read_demuxer_, 0, false, ec);
                    }
                    read_demuxer_.stream->seek(position, offset);
                } else {
                    boost::uint64_t head_length = position.source->segment_head_size(position.segment);
                    if (head_length && time) {
                        read_demuxer_.stream->seek(position, 0, head_length);
                    } else {
                        read_demuxer_.stream->seek(position, 0);
                    }
                }
                boost::system::error_code ec1;
                create_demuxer(buffer_->write_segment(), write_demuxer_, ec1);
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
            root_source_->on_error(ec);
            last_error(ec);
            return ec;
        }

        boost::system::error_code BufferDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (!events_->events.empty()) { // 双保险
                handle_events();
            }
            tick_on();
            read_demuxer_.stream->read_more(0);
            if (seek_time_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                return ec;
            }
            // 丢掉上一次的数据
            read_demuxer_.stream->drop();
            while (read_demuxer_.demuxer->get_sample(sample, ec)) {
                if (ec == ppbox::demux::error::file_stream_error
                    || ec == error::no_more_sample) {
                    if (buffer_->read_segment() != buffer_->write_segment()) {
                        std::cout << "finish segment " << buffer_->read_segment().segment << std::endl;
                        boost::uint32_t cur_time = read_demuxer_.demuxer->get_cur_time(ec);
                        read_demuxer_.stream->drop_all();
                        if (buffer_->read_segment().source) {
                            size_t insert_time = read_demuxer_.segment.source->insert_time();
                            change_source(const_cast<SegmentPositionEx &>(buffer_->read_segment()), 
                                read_demuxer_, true, ec);
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
                        ec = read_demuxer_.stream->error();
                        // TODO: 什么情况下no_more_sample?
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
                        last_error(ec);
                        break;
                    }
                }
            } else {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                last_error(ec);
            }
            return ec;
        }

        size_t BufferDemuxer::get_media_count(
            boost::system::error_code & ec)
        {
            size_t n = read_demuxer_.demuxer->get_media_count(ec);
            last_error(ec);
            return n;
        }

        boost::system::error_code BufferDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
        {
            read_demuxer_.demuxer->get_media_info(index, info, ec);
            last_error(ec);
            return ec;
        }

        boost::uint32_t BufferDemuxer::get_duration(
            boost::system::error_code & ec)
        {
            boost::uint32_t d = read_demuxer_.demuxer->get_duration(ec);
            last_error(ec);
            return d;
        }

        void BufferDemuxer::segment_write_beg(
            SegmentPositionEx & segment)
        {
            boost::system::error_code ec;
            change_source(segment, write_demuxer_, false, ec);
        }

        boost::system::error_code BufferDemuxer::cancel(
            boost::system::error_code & ec)
        {
            return ec;
        }

        boost::system::error_code BufferDemuxer::close(
            boost::system::error_code & ec)
        {
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
                        reload_demuxer(old_source->insert_demuxer().demuxer, new_segment, demuxer, old_source->insert_input_time(), is_seek, ec);
                    } else {
                        SourceBase * prev_sibling = old_source->prev_sibling();
                        bool flag = false;
                        while (prev_sibling ) {
                            if (prev_sibling->insert_demuxer().demuxer
                                && prev_sibling->insert_segment() == old_source->insert_segment()) {
                                    flag = true;
                                    reload_demuxer(prev_sibling->insert_demuxer().demuxer, new_segment, demuxer, old_source->insert_input_time(), is_seek, ec);
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

        void BufferDemuxer::handle_async(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (!ec) {// Jump中已经有一个分段
                read_demuxer_.stream->update_new(buffer_->read_segment());
                if (read_demuxer_.demuxer->is_open(ec)) {
                    read_demuxer_.stream->drop();
                    if (seek_time_ && seek(seek_time_, ec)) {
                    }
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    boost::uint64_t head_length = 
                        buffer_->read_segment().source->segment_head_size(buffer_->read_segment().segment);
                    if (head_length && buffer_->read_back() < head_length) {
                        buffer_->async_prepare(
                            head_length - buffer_->read_back(), 
                            boost::bind(&BufferDemuxer::handle_async, this, _1));
                    } else {
                        buffer_->async_prepare_at_least(0, 
                            boost::bind(&BufferDemuxer::handle_async, this, _1));
                    }
                    return;
                }
            }
            open_end();
            if (!ec) {
                size_t stream_count = read_demuxer_.demuxer->get_media_count(ec);
                for(size_t i = 0; i < stream_count; i++) {
                    MediaInfo info;
                    read_demuxer_.demuxer->get_media_info(i, info, ec);
                    media_time_scales_.push_back(info.time_scale);
                    dts_offset_.push_back(info.time_scale);
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

        void BufferDemuxer::create_demuxer(
            SegmentPositionEx const & segment, 
            DemuxerInfo & demuxer, 
            boost::system::error_code & ec)
        {
            ec.clear();
            if (segment == read_demuxer_.segment) {
                demuxer.stream = read_demuxer_.stream;
                demuxer.demuxer = read_demuxer_.demuxer;
                demuxer.segment = segment;
            } else if (segment == write_demuxer_.segment) {
                demuxer.stream = write_demuxer_.stream;
                demuxer.demuxer = write_demuxer_.demuxer;
                demuxer.segment = segment;
            } else {
                demuxer.segment = segment;
                BytesStream * stream = new BytesStream(*buffer_, const_cast<SegmentPositionEx &>(segment));
                stream->update_new(segment);
                demuxer.stream.reset(stream);
                demuxer.demuxer.reset(ppbox::demux::create_demuxer(segment.source->demuxer_type(), *demuxer.stream));
                demuxer.demuxer->open(ec);
            }
        }

        void BufferDemuxer::reload_demuxer(
            DemuxerPointer & demuxer, 
            SegmentPositionEx & segment, 
            DemuxerInfo & demuxer_info, 
            boost::uint32_t time, 
            bool is_seek, 
            boost::system::error_code & ec)
        {
            if (segment == read_demuxer_.segment) {
                demuxer_info.stream = read_demuxer_.stream;
            } else if (segment == write_demuxer_.segment) {
                demuxer_info.stream = write_demuxer_.stream;
            } else {
                BytesStream * stream = new BytesStream(*buffer_, const_cast<SegmentPositionEx &>(segment));
                stream->update_new(segment);
                demuxer_info.stream.reset(stream);
            }
            demuxer_info.demuxer.reset(demuxer->clone(* demuxer_info.stream));
            demuxer_info.segment = segment;
            if (is_seek) {
                demuxer_info.demuxer->seek(time, ec);
            }
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

        void BufferDemuxer::on_error(
            boost::system::error_code & ec)
        {
            if (ec == source_error::at_end_point) {
                write_demuxer_.stream->update_new(
                    buffer_->write_segment());
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
                    //    ec = ec_buf;s
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

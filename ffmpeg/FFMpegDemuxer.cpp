// FFMpegDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/ffmpeg/FFMpegDemuxer.h"
#include "ppbox/demux/ffmpeg/FFMpegProto.h"
#include "ppbox/demux/basic/BasicDemuxer.h"

#include <ppbox/data/base/MediaBase.h>
#include <ppbox/data/base/Error.h>
#include <ppbox/data/single/SingleSource.h>
#include <ppbox/data/single/SingleBuffer.h>

#include <ppbox/avformat/Format.h>
using namespace ppbox::avformat::error;
#include <ppbox/avcodec/Codec.h>
#include <ppbox/avcodec/ffmpeg/FFMpegLog.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/ErrorCode.h>

#include <boost/bind.hpp>
#include <boost/dynamic_bitset.hpp>

#include <bitset>

extern "C" {
#define UINT64_C(c)   c ## ULL
#define INT64_MIN     (-INT64_C(9223372036854775807-1))
#define INT64_MAX     (INT64_C(9223372036854775807))
#include <libavformat/avformat.h>
}

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.FFMpegDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        FFMpegDemuxer::FFMpegDemuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::MediaBase & media)
            : Demuxer(io_svc)
            , media_(media)
            , source_(NULL)
            , buffer_(NULL)
            , mem_lock_pool_(framework::memory::PrivateMemory(), 1024 * 1024, sizeof(ppbox::data::MemoryLock))
            , avf_ctx_(NULL)
            , start_time_(0)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(closed)
        {
            ppbox::avcodec::ffmpeg_log_setup();
            av_register_all();
        }

        FFMpegDemuxer::~FFMpegDemuxer()
        {
        }

        boost::system::error_code FFMpegDemuxer::open (
            boost::system::error_code & ec)
        {
            return Demuxer::open(ec);
        }

        void FFMpegDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            handle_async_open(ec);
        }

        bool FFMpegDemuxer::is_open(
            boost::system::error_code & ec) const
        {
            if (open_state_ == opened) {
                ec.clear();
                return true;
            } else if (open_state_ == closed) {
                ec = error::not_open;
                return false;
            } else {
                ec = boost::asio::error::would_block;
                return false;
            }
        }

        bool FFMpegDemuxer::is_open(
            boost::system::error_code & ec)
        {
            return const_cast<FFMpegDemuxer const *>(this)->is_open(ec);
        }

        boost::system::error_code FFMpegDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (media_open == open_state_) {
                media_.cancel(ec);
            } else if (demuxer_open == open_state_) {
                source_->cancel(ec);
            }
            return ec;
        }

        boost::system::error_code FFMpegDemuxer::close(
            boost::system::error_code & ec)
        {
            DemuxStatistic::close();
            if (open_state_ > demuxer_open) {
                on_close();
                avformat_close_input(&avf_ctx_);
            }
            if (open_state_ > media_open) {
                media_.close(ec);
            }
            seek_pending_ = false;
            seek_time_ = 0;
            open_state_ = closed;

            if (buffer_) {
                remove_buffer(*buffer_);
                delete buffer_;
                buffer_ = NULL;
            }
            if (source_) {
                source_->close(ec);
                util::stream::UrlSource * source = const_cast<util::stream::UrlSource *>(&source_->source());
                util::stream::UrlSourceFactory::destroy(source);
                delete source_;
                source_ = NULL;
            }

            return ec;
        }

        void FFMpegDemuxer::handle_async_open(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (ec) {
                DemuxStatistic::last_error(ec);
                resp_(ec);
                return;
            }

            switch(open_state_) {
                case closed:
                    open_state_ = media_open;
                    DemuxStatistic::open_beg_media();
                    media_.async_open(
                        boost::bind(&FFMpegDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    media_.get_info(media_info_, ec);
                    media_.get_url(url_, ec);
                    if (!ec) {
                        util::stream::UrlSource * source = 
                            util::stream::UrlSourceFactory::create(get_io_service(), media_.get_protocol(), ec);
                        if (source) {
                            boost::system::error_code ec1;
                            source->set_non_block(true, ec1);
                            source_ = new ppbox::data::SingleSource(url_, *source);
                            source_->set_time_out(5000);
                            buffer_ = new ppbox::data::SingleBuffer(*source_, 10 * 1024 * 1024, 10240);
                            insert_buffer(*buffer_);
                            // TODO:
                            open_state_ = demuxer_open;
                            DemuxStatistic::open_beg_demux();
                            buffer_->pause_stream();
                            buffer_->seek(0, ec);
                            buffer_->pause_stream();
                        }
                    }
                case demuxer_open:
                    if (!ec && avformat_open(ec)) {
                        open_state_ = opened;
                        on_open();
                        open_end();
                        response(ec);
                    } else if (ec == boost::asio::error::try_again && buffer_->out_position() < 1024 * 1024) {
                        buffer_->async_prepare_some(0, 
                            boost::bind(&FFMpegDemuxer::handle_async_open, this, _1));
                    } else {
                        open_end();
                        response(ec);
                    }
                    break;
                default:
                    assert(0);
                    break;
            }
        }

        void FFMpegDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

        boost::system::error_code FFMpegDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            while (!peek_packets_.empty()) {
                av_free_packet(peek_packets_.front());
                peek_packets_.pop_front();
            }
            int64_t ts = time * (AV_TIME_BASE / 1000) + start_time_;
            int result = avformat_seek_file(avf_ctx_, -1, INT64_MIN, ts, INT64_MAX, 0);
            seek_time_ = time;
            if (ec == boost::asio::error::would_block) {
                seek_pending_ = true;
            } else {
                seek_pending_ = false;
            }
            if (&time != &seek_time_ && open_state_ == opened) {
                DemuxStatistic::seek(!ec, time);
            }
            if (ec) {
                DemuxStatistic::last_error(ec);
            }
            return ec;
        }

        boost::uint64_t FFMpegDemuxer::check_seek(
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

        boost::system::error_code FFMpegDemuxer::pause(
            boost::system::error_code & ec)
        {
            source_->pause();
            DemuxStatistic::pause();
            ec.clear();
            return ec;
        }

        boost::system::error_code FFMpegDemuxer::resume(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            DemuxStatistic::resume();
            return ec;
        }

        boost::system::error_code FFMpegDemuxer::get_media_info(
            ppbox::data::MediaInfo & info,
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                info = media_info_;
            }
            return ec;
        }

        size_t FFMpegDemuxer::get_stream_count(
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                return streams_.size();
            }
            return 0;
        }

        boost::system::error_code FFMpegDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                if (index >= streams_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_[index];
                }
            }
            return ec;
        }

        bool FFMpegDemuxer::fill_data(
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            return !ec;
        }

        bool FFMpegDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                //CustomDemuxer::get_stream_status(info, ec);
                info.byte_range.end = media_info_.file_size;
                return !ec;
            }
            return false;
        }

        bool FFMpegDemuxer::get_data_stat(
            DataStat & stat, 
            boost::system::error_code & ec) const
        {
            if (source_) {
                stat = *source_;
                ec.clear();
            } else {
                ec = error::not_open;
            }
            return !ec;
        }

        boost::system::error_code FFMpegDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            buffer_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                return ec;
            }
            assert(!seek_pending_);

            if (sample.memory) {
                free_packet(sample.memory);
                sample.memory = NULL;
            }
            sample.data.clear();

            AVPacket * pkt = NULL;
            if (!peek_packets_.empty()) {
                pkt = peek_packets_.front();
                peek_packets_.pop_front();
                ec.clear();
            } else {
                pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
                av_init_packet(pkt);
                int result = av_read_frame(avf_ctx_, pkt);
                if (result < 0 || (pkt->flags & AV_PKT_FLAG_CORRUPT)) {
                    ec = buffer_->last_error();
                    assert(ec);
                    if (ec == boost::asio::error::eof)
                        ec = end_of_stream;
                    if (!ec) {
                        ec = boost::asio::error::would_block;
                    }
                    return ec;
                } else {
                    ec.clear();
                }
            }

            sample.itrack = pkt->stream_index;
            sample.flags = 0;
            if (pkt->flags & AV_PKT_FLAG_KEY) {
                sample.flags |= sample.f_sync;
            }
            sample.dts = pkt->dts;
            sample.cts_delta = pkt->pts - pkt->dts;
            sample.duration = pkt->duration;
            sample.size = pkt->size;
            sample.data.push_back(boost::asio::buffer(pkt->data, pkt->size));
            sample.stream_info = &streams_[sample.itrack];
            ppbox::data::MemoryLock * lock = (ppbox::data::MemoryLock *)mem_lock_pool_.alloc();
            new (lock) ppbox::data::MemoryLock;
            lock->pointer = pkt;
            sample.memory = lock;

            Demuxer::adjust_timestamp(sample);

            return ec;
        }

        void FFMpegDemuxer::free_packet(
            ppbox::data::MemoryLock * lock)
        {
            while (!lock->join.empty()) {
                ppbox::data::MemoryLock * l = lock->join.first();
                l->unlink();
                free_packet(l);
            }
            AVPacket * pkt = (AVPacket *)lock->pointer;
            av_free_packet(pkt);
            av_free(pkt);
            mem_lock_pool_.free(lock);
        }

        bool FFMpegDemuxer::free_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (sample.memory) {
                ppbox::data::MemoryLock * lock = (ppbox::data::MemoryLock *)sample.memory;
                free_packet(lock);
                sample.memory = NULL;
            }
            ec.clear();
            return true;
        }

        bool FFMpegDemuxer::avformat_open(
            boost::system::error_code & ec)
        {
            int result = avformat_open_input(&avf_ctx_, buffer_url(*buffer_).c_str(), NULL, NULL);
            if (result == 0)
                result = avformat_find_stream_info(avf_ctx_, NULL);
            if (result < 0) {
                ec = boost::system::error_code(-result, boost::system::get_system_category());
                return false;
            }
            media_info_.duration = avf_ctx_->duration / (AV_TIME_BASE / 1000);
            streams_.resize(avf_ctx_->nb_streams);
            boost::dynamic_bitset<> config_readys;
            boost::dynamic_bitset<> time_readys;
            config_readys.resize(avf_ctx_->nb_streams);
            time_readys.resize(avf_ctx_->nb_streams);
            for (unsigned int i = 0; i < avf_ctx_->nb_streams; ++i) {
                StreamInfo & stream1 = streams_[i];
                AVStream * stream2 = avf_ctx_->streams[i];
                switch (stream2->codec->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    stream1.type = StreamType::VIDE;
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    stream1.type = StreamType::AUDI;
                    break;
                default:
                    stream1.type = StreamType::NONE;
                    break;
                }
                switch (stream1.type) {
                case StreamType::VIDE:
                    stream1.video_format.width = stream2->codec->width;
                    stream1.video_format.height = stream2->codec->height;
                    stream1.video_format.frame_rate_num = stream2->codec->time_base.den;
                    stream1.video_format.frame_rate_den = stream2->codec->time_base.num * stream2->codec->ticks_per_frame;
                    break;
                case StreamType::AUDI:
                    stream1.audio_format.channel_count = stream2->codec->channels;
                    stream1.audio_format.sample_size = stream2->codec->bits_per_coded_sample;
                    stream1.audio_format.sample_rate = stream2->codec->sample_rate;
                    stream1.audio_format.block_align = stream2->codec->block_align;
                    stream1.audio_format.sample_per_frame = stream2->codec->frame_size;
                }
                stream1.index = i;
                stream1.time_scale = stream2->time_base.den / stream2->time_base.num;
                stream1.bitrate = stream2->codec->bit_rate;
                //stream1.start_time = stream2->start_time;
                if (stream2->start_time != AV_NOPTS_VALUE && stream1.type != StreamType::VIDE) {
                    stream1.start_time = stream2->start_time; // we expect dts, but this is pts, so discard wideo's value
                    time_readys.set(i);
                }
                stream1.duration = stream2->duration;
                stream1.format_data.assign(stream2->codec->extradata, stream2->codec->extradata + stream2->codec->extradata_size);
                if (ppbox::avformat::Format::finish_from_stream(stream1, "ffmpeg", stream2->codec->codec_id, ec)) {
                    config_readys.set(i);
                }
            }
            while (time_readys.count() != time_readys.size() 
                || config_readys.count() != config_readys.size()) {
                    AVPacket * pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
                    av_init_packet(pkt);
                    result = av_read_frame(avf_ctx_, pkt);
                    if (!time_readys.test(pkt->stream_index)) {
                        streams_[pkt->stream_index].start_time = pkt->dts;
                        time_readys.set(pkt->stream_index);
                    }
                    if (!config_readys.test(pkt->stream_index)) {
                        streams_[pkt->stream_index].format_data.assign(pkt->data, pkt->data + pkt->size);
                        if (ppbox::avcodec::Codec::static_finish_stream_info(streams_[pkt->stream_index], ec)) {
                            config_readys.set(pkt->stream_index);
                        }
                    }
                    peek_packets_.push_back(pkt);
            }
            start_time_ = streams_[0].start_time * AV_TIME_BASE / streams_[0].time_scale; // AV_TIME_BASE units
            for (size_t i = 1; i < streams_.size(); ++i) {
                if (streams_[i].start_time * AV_TIME_BASE < start_time_ * streams_[i].time_scale)
                    start_time_ = streams_[i].start_time * AV_TIME_BASE / streams_[i].time_scale;
            }
            for (size_t i = 0; i < streams_.size(); ++i) {
                if ((boost::int64_t)streams_[i].start_time >= 0)
                    streams_[i].start_time = start_time_ * streams_[i].time_scale / AV_TIME_BASE;
            }
            return true;
        }

    } // namespace demux
} // namespace ppbox

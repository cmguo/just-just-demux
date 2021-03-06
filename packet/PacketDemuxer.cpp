// PacketDemuxer.cpp

#include "just/demux/Common.h"
#include "just/demux/packet/PacketDemuxer.h"
#include "just/demux/packet/filter/SourceFilter.h"
#include "just/demux/packet/filter/TimestampFilter.h"
#include "just/demux/packet/filter/SortFilter.h"
#include "just/demux/base/DemuxError.h"

using namespace just::avformat::error;

#include <just/data/packet/PacketMedia.h>
#include <just/data/packet/PacketBuffer.h>

#include <util/stream/Source.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/LogicError.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.PacketDemuxer", framework::logger::Debug);

namespace just
{
    namespace demux
    {

        PacketDemuxer::PacketDemuxer(
            boost::asio::io_service & io_svc, 
            just::data::PacketMedia & media)
            : Demuxer(io_svc)
            , media_(media)
            , source_(NULL)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(not_open)
        {
        }

        PacketDemuxer::~PacketDemuxer()
        {
            boost::system::error_code ec;
            while (!filters_.empty()) {
                delete filters_.last();
            }
            if (source_) {
                delete source_;
                source_ = NULL;
            }
        }

        void PacketDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            handle_async_open(ec);
        }

        bool PacketDemuxer::is_open(
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

        bool PacketDemuxer::is_open(
            boost::system::error_code & ec)
        {
            return const_cast<PacketDemuxer const *>(this)->is_open(ec);
        }

        boost::system::error_code PacketDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (media_open == open_state_) {
                media_.cancel(ec);
            } else if (demuxer_open == open_state_) {
                //source_->cancel(ec);
            }
            return ec;
        }

        boost::system::error_code PacketDemuxer::close(
            boost::system::error_code & ec)
        {
            DemuxStatistic::close();
            Sample sample;
            while (!peek_samples_.empty()) {
                sample.append(peek_samples_.front());
                peek_samples_.pop_front();
            }
            if (!filters_.empty()) { // 可能没有打开成功
                filters_.last()->before_seek(sample, ec); // will call source_->putback()
                delete filters_.first(); // SourceFilter
                filters_.clear();
            }
            media_.close(ec);
            seek_time_ = 0;
            seek_pending_ = false;
            open_state_ = not_open;
            return ec;
        }

        void PacketDemuxer::handle_async_open(
            boost::system::error_code const & ecc)
        {
            boost::system::error_code ec = ecc;
            if (ec) {
                last_error(ec);
                response(ec);
                return;
            }

            switch(open_state_) {
                case not_open:
                    open_state_ = media_open;
                    DemuxStatistic::open_beg_media();
                    media_.async_open(
                        boost::bind(&PacketDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    media_.get_info(media_info_, ec);
                    if (!ec) {
                        just::data::PacketFeature feature;
                        media_.get_packet_feature(feature, ec);
                        source_ = new just::data::PacketSource(feature, media_.source());
                        boost::system::error_code ec1;
                        media_.source().set_non_block(true, ec1);
                        filters_.push_back(new SourceFilter(*source_));
                        open_state_ = demuxer_open;
                        DemuxStatistic::open_beg_stream();
                    }
                case demuxer_open:
                    source_->pause_stream();
                    if (!ec && check_open(ec)) {
                        open_state_ = open_finished;
                        if (!media_info_.flags & just::data::PacketMediaFlags::f_has_time) {
                            filters_.push_back(new TimestampFilter(timestamp()));
                        }
                        if (media_info_.flags & just::data::PacketMediaFlags::f_non_ordered) {
                            filters_.push_back(new SortFilter(stream_infos_.size()));
                        }
                        on_open();
                        DemuxStatistic::open_end();
                        source_->resume_stream();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block) {
                        source_->async_prepare(
                            boost::bind(&PacketDemuxer::handle_async_open, this, _1));
                    } else {
                        open_state_ = open_finished;
                        DemuxStatistic::open_end();
                        response(ec);
                    }
                    break;
                default:
                    assert(0);
                    break;
            }
        }

        bool PacketDemuxer::check_open(
            boost::system::error_code & ec)
        {
            ec.clear();
            return true;
        }

        void PacketDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

        boost::system::error_code PacketDemuxer::get_media_info(
            just::data::MediaInfo & info,
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                media_.get_info(info, ec);
                info.duration = media_info_.duration;
            }
            return ec;
        }

        size_t PacketDemuxer::get_stream_count(
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                return stream_infos_.size();
            }
            return 0;
        }

        boost::system::error_code PacketDemuxer::get_stream_info(
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

        boost::system::error_code PacketDemuxer::reset(
            boost::system::error_code & ec)
        {
            return ec;
        }

        boost::system::error_code PacketDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            Sample sample;
            sample.time = time;
            filters_.last()->before_seek(sample, ec);
            seek_time_ = time;
            if (ec == boost::asio::error::would_block) {
                seek_pending_ = true;
            } else {
                seek_pending_ = false;
            }
            if (&time != &seek_time_ && open_state_ == open_finished) {
                DemuxStatistic::seek(!ec, time);
            }
            if (ec) {
                DemuxStatistic::last_error(ec);
            }
            return ec;
        }

        boost::uint64_t PacketDemuxer::check_seek(
            boost::system::error_code & ec)
        {
            if (seek_pending_) {
                seek(seek_time_, ec);
            } else {
                ec.clear();
            }
            return seek_time_;
        }

        boost::system::error_code PacketDemuxer::pause(
            boost::system::error_code & ec)
        {
            DemuxStatistic::pause();
            ec.clear();
            return ec;
        }

        boost::system::error_code PacketDemuxer::resume(
            boost::system::error_code & ec)
        {
            DemuxStatistic::resume();
            return ec;
        }

        bool PacketDemuxer::fill_data(
            boost::system::error_code & ec)
        {
            source_->prepare_some(ec);
            return !ec;
        }

        bool PacketDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            using just::data::invalid_size;

            if (is_open(ec)) {
                info.byte_range.beg = 0;
                info.byte_range.end = invalid_size;
                info.byte_range.pos = source_->in_position();
                info.byte_range.buf = source_->out_position();

                info.time_range.beg = 0;
                info.time_range.end = media_info_.duration;
                info.time_range.pos = get_cur_time(ec);
                info.time_range.buf = get_end_time(ec);

                info.buf_ec = source_->last_error();
                return !ec;
            }
            return false;
        }

        bool PacketDemuxer::get_data_stat(
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

        boost::system::error_code PacketDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            source_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                return ec;
            }
            assert(!seek_pending_);

            free_sample(sample, ec);

            get_sample2(sample, ec);

            if (!ec) {
                DemuxStatistic::play_on(sample.time);
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                } else if (ec == boost::asio::error::eof) {
                    ec = end_of_stream;
                }
                last_error(ec);
            }
            return ec;
        }

        bool PacketDemuxer::free_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (sample.memory) {
                source_->putback(sample.memory);
                sample.memory = NULL;
            }
            ec.clear();
            return true;
        }

        boost::uint64_t PacketDemuxer::get_cur_time(
            boost::system::error_code & ec)
        {
            Sample sample;
            if (!filters_.last()->get_next_sample(sample, ec)) {
                sample.time = seek_time_;
            }
            return sample.time;
        }

        boost::uint64_t PacketDemuxer::get_end_time(
            boost::system::error_code & ec)
        {
            Sample sample;
            if (!filters_.last()->get_last_sample(sample, ec)) {
                sample.time = seek_time_;
            }
            return sample.time;
        }

        void PacketDemuxer::add_filter(
            Filter * filter)
        {
            filters_.push_back(filter);
        }

        void PacketDemuxer::get_sample2(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            while (!peek_samples_.empty()) {
                sample = peek_samples_.front();
                peek_samples_.pop_front();
                ec.clear();
                if ((sample.flags & sample.f_config) == 0)
                    return;
                free_sample(sample, ec);
            }
            while (true) {
                filters_.last()->get_sample(sample, ec);
                if (ec || (sample.flags & sample.f_config) == 0)
                    break;
                free_sample(sample, ec);
            }
        }

        bool PacketDemuxer::peek_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            filters_.last()->get_sample(sample, ec);
            if (!ec) {
                peek_samples_.push_back(sample);
                return true;
            }
            return false;
        }

        void PacketDemuxer::drop_sample()
        {
            boost::system::error_code ec;
            free_sample(peek_samples_.back(), ec);
            peek_samples_.pop_back();
        }

        boost::system::error_code PacketDemuxerTraits::error_not_found()
        {
            return error::not_support;
        }

    } // namespace demux
} // namespace just

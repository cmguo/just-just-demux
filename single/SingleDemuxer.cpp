// SingleDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/single/SingleDemuxer.h"
#include "ppbox/demux/basic/BasicDemuxer.h"

#include <ppbox/data/base/MediaBase.h>
#include <ppbox/data/base/SourceError.h>
#include <ppbox/data/base/UrlSource.h>
#include <ppbox/data/single/SingleSource.h>
#include <ppbox/data/single/SourceStream.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/timer/Ticker.h>

#include <boost/thread/condition_variable.hpp>
#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.SingleDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        SingleDemuxer::SingleDemuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::MediaBase & media)
            : CustomDemuxer(*create_demuxer(io_svc, media))
            , media_(media)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(not_open)
        {
        }

        SingleDemuxer::~SingleDemuxer()
        {
            boost::system::error_code ec;
            if (stream_) {
                delete stream_;
                stream_ = NULL;
            }
            if (source_) {
                ppbox::data::UrlSource * source = (ppbox::data::UrlSource *)&source_->source();
                ppbox::data::UrlSource::destroy(source);
                delete source_;
                source_ = NULL;
            }
            delete &CustomDemuxer::detach();
        }

        void SingleDemuxer::async_open(
            open_response_type const & resp)
        {
            resp_ = resp;
            boost::system::error_code ec;
            handle_async_open(ec);
        }

        bool SingleDemuxer::is_open(
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

        bool SingleDemuxer::is_open(
            boost::system::error_code & ec)
        {
            return const_cast<SingleDemuxer const *>(this)->is_open(ec);
        }

        boost::system::error_code SingleDemuxer::cancel(
            boost::system::error_code & ec)
        {
            if (media_open == open_state_) {
                media_.cancel(ec);
            } else if (demuxer_open == open_state_) {
                source_->cancel(ec);
            }
            return ec;
        }

        boost::system::error_code SingleDemuxer::close(
            boost::system::error_code & ec)
        {
            DemuxStatistic::close();
            CustomDemuxer::close(ec);
            media_.close(ec);
            seek_time_ = 0;
            open_state_ = not_open;
            return ec;
        }

        DemuxerBase * SingleDemuxer::create_demuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::MediaBase & media)
        {
            ppbox::data::UrlSource * source = 
                ppbox::data::UrlSource::create(io_svc, media.get_protocol());
            error_code ec;
            source->set_non_block(true, ec);
            source_ = new ppbox::data::SingleSource(url_, *source);
            source_->set_time_out(5000);
            stream_ = new ppbox::data::SourceStream(*source_, 10 * 1024 * 1024, 10240);
            ppbox::data::MediaBasicInfo info;
            media.get_basic_info(info, ec);
            BasicDemuxer * demuxer = BasicDemuxer::create(info.format, io_svc, *stream_);
            return demuxer;
        }

        void SingleDemuxer::handle_async_open(
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
                    open_state_ = media_open;
                    DemuxStatistic::open_beg();
                    media_.async_open(
                        boost::bind(&SingleDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    open_state_ = demuxer_open;
                    media_.get_info(media_info_, ec);
                    media_.get_url(url_, ec);
                    if (!ec) {
                        // TODO:
                        stream_->pause_stream();
                        stream_->seek(0, ec);
                        stream_->pause_stream();
                        CustomDemuxer::open(ec);
                        CustomDemuxer::reset(ec);
                    }
                case demuxer_open:
                    stream_->pause_stream();
                    if (!ec && !seek(seek_time_, ec)) { // 上面的reset可能已经有错误，所以判断ec
                        open_state_ = open_finished;
                        stream_->set_track_count(get_stream_count(ec));
                        open_end();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block || (ec == error::file_stream_error 
                        && stream_->last_error() == boost::asio::error::would_block)) {
                            stream_->async_prepare_some(0, 
                                boost::bind(&SingleDemuxer::handle_async_open, this, _1));
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

        void SingleDemuxer::response(
            boost::system::error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

        boost::system::error_code SingleDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            CustomDemuxer::seek(time, ec);
            seek_time_ = time;
            if (ec == boost::asio::error::would_block) {
                seek_pending_ = true;
            } else {
                seek_pending_ = false;
            }
            if (&time != &seek_time_ && open_state_ == open_finished) {
                DemuxStatistic::seek(!ec, time);
            }
            return ec;
        }

        boost::uint64_t SingleDemuxer::check_seek(
            boost::system::error_code & ec)
        {
            stream_->prepare_some(ec);
            if (seek_pending_) {
                seek(seek_time_, ec);
            } else {
                ec.clear();
            }
            return seek_time_;
        }

        boost::system::error_code SingleDemuxer::pause(
            boost::system::error_code & ec)
        {
            source_->pause();
            DemuxStatistic::pause();
            ec.clear();
            return ec;
        }

        boost::system::error_code SingleDemuxer::resume(
            boost::system::error_code & ec)
        {
            stream_->prepare_some(ec);
            DemuxStatistic::resume();
            return ec;
        }

        boost::system::error_code SingleDemuxer::get_media_info(
            ppbox::data::MediaInfo & info,
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                ppbox::data::MediaInfo info1;
                CustomDemuxer::get_media_info(info1, ec);
                media_.get_info(info, ec);
                if (info.duration == ppbox::data::invalid_size) {
                    info.duration = info1.duration;
                }
            }
            return ec;
        }

        bool SingleDemuxer::fill_data(
            boost::system::error_code & ec)
        {
            stream_->prepare_some(ec);
            return !ec;
        }

        bool SingleDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                CustomDemuxer::get_stream_status(info, ec);
                info.byte_range.end = media_info_.file_size;
                return !ec;
            }
            return false;
        }

        bool SingleDemuxer::get_data_stat(
            DataStatistic & stat, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                stat = *source_;
                return true;
            }
            return false;
        }

        boost::system::error_code SingleDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            stream_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                return ec;
            }
            assert(!seek_pending_);

            if (sample.memory) {
                stream_->putback(sample.memory);
                sample.memory = NULL;
            }

            sample.data.clear();
            CustomDemuxer::get_sample(sample, ec);
            if (ec == error::file_stream_error) {
                ec = stream_->last_error();
                assert(ec);
                if (ec == boost::asio::error::eof)
                    ec = error::no_more_sample;
                if (!ec) {
                    ec = boost::asio::error::would_block;
                }
            }
            if (!ec) {
                DemuxStatistic::play_on(sample.time);
                sample.memory = stream_->fetch(sample.itrack, *(std::vector<ppbox::data::DataBlock> *)sample.context, sample.data, ec);
                assert(!ec);
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                last_error(ec);
            }
            return ec;
        }

        bool SingleDemuxer::free_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (sample.memory) {
                stream_->putback(sample.memory);
                sample.memory = NULL;
            }
            ec.clear();
            return true;
        }

    } // namespace demux
} // namespace ppbox

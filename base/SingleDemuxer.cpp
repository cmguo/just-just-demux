// SingleDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SingleDemuxer.h"
#include "ppbox/demux/base/Demuxer.h"

#include <ppbox/data/base/MediaBase.h>
#include <ppbox/data/base/SourceError.h>
#include <ppbox/data/base/SourceBase.h>
#include <ppbox/data/base/SingleSource.h>
#include <ppbox/data/base/SourceStream.h>

using namespace ppbox::avformat;

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
            ticker_ = new framework::timer::Ticker(1000);
        }

        SingleDemuxer::~SingleDemuxer()
        {
            boost::system::error_code ec;
            close(ec);
            if (stream_) {
                delete stream_;
                stream_ = NULL;
            }
            if (source_) {
                ppbox::data::SourceBase * source = (ppbox::data::SourceBase *)&source_->source();
                ppbox::data::SourceBase::destroy(source);
                delete source_;
                source_ = NULL;
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

        boost::system::error_code SingleDemuxer::open(
            boost::system::error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(boost::ref(resp));
            resp.wait();
            return ec;
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
            cancel(ec);
            seek_time_ = 0;
            open_state_ = not_open;
            return ec;
        }

        DemuxerBase * SingleDemuxer::create_demuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::MediaBase & media)
        {
            ppbox::data::SourceBase * source = 
                ppbox::data::SourceBase::create(io_svc, media);
            error_code ec;
            source->set_non_block(true, ec);
            source_ = new ppbox::data::SingleSource(url_, *source);
            stream_ = new ppbox::data::SourceStream(*source_, 10 * 1024 * 1024, 10240);
            ppbox::data::MediaBasicInfo info;
            media.get_basic_info(info, ec);
            Demuxer * demuxer = Demuxer::create(info.format, io_svc, *stream_);
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
            if (ec == boost::asio::error::would_block) {
                seek_pending_ = true;
            } else {
                seek_pending_ = false;
            }
            return ec;
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

        boost::system::error_code SingleDemuxer::get_play_info(
            PlayInfo & info, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
            }
            return ec;
        }

        bool SingleDemuxer::get_data_stat(
            DataStatistic & stat, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                stat = *source_;
            }
            return !ec;
        }

        boost::system::error_code SingleDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            stream_->drop(ec);
            stream_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    block_on();
                }
                return ec;
            }
            assert(!seek_pending_);

            CustomDemuxer::get_sample(sample, ec);
            if (ec == error::file_stream_error) {
                ec = stream_->last_error();
                assert(ec);
                if (ec == ppbox::data::source_error::no_more_segment)
                    ec = error::no_more_sample;
                if (!ec) {
                    ec = boost::asio::error::would_block;
                }
            }
            if (!ec) {
                play_on(sample.time);
                sample.data.clear();
                for (size_t i = 0; i < sample.blocks.size(); ++i) {
                    stream_->fetch(sample.blocks[i].offset, sample.blocks[i].size, sample.data, ec);
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

        void SingleDemuxer::on_event(
            util::event::Event const & event)
        {
        }

    } // namespace demux
} // namespace ppbox

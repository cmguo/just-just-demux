// SingleDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/single/SingleDemuxer.h"
#include "ppbox/demux/basic/BasicDemuxer.h"

#include <ppbox/data/base/MediaBase.h>
#include <ppbox/data/base/Error.h>
#include <ppbox/data/single/SingleSource.h>
#include <ppbox/data/single/SourceStream.h>

#include <ppbox/avformat/Format.h>
using namespace ppbox::avformat::error;

#include <util/stream/UrlSource.h>

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
            : CustomDemuxer(io_svc)
            , media_(media)
            , source_(NULL)
            , stream_(NULL)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(closed)
        {
        }

        SingleDemuxer::~SingleDemuxer()
        {
        }

        boost::system::error_code SingleDemuxer::open (
            boost::system::error_code & ec)
        {
            return Demuxer::open(ec);
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
            if (open_state_ > demuxer_probe) {
                CustomDemuxer::close(ec);
            }
            if (open_state_ > media_open) {
                media_.close(ec);
            }
            seek_pending_ = false;
            seek_time_ = 0;
            open_state_ = closed;

            if (stream_) {
                delete stream_;
                stream_ = NULL;
            }
            if (source_) {
                source_->close(ec);
                util::stream::UrlSource * source = const_cast<util::stream::UrlSource *>(&source_->source());
                util::stream::UrlSource::destroy(source);
                delete source_;
                source_ = NULL;
            }
            delete &CustomDemuxer::detach();

            return ec;
        }

        bool SingleDemuxer::create_demuxer(
            boost::system::error_code & ec)
        {
            if (!media_info_.format.empty()) {
                BasicDemuxer * demuxer = BasicDemuxer::create(media_info_.format, get_io_service(), *stream_, ec);
                if (demuxer) {
                    attach(*demuxer);
                    return true;
                }
            } else {
                BasicDemuxer * demuxer = BasicDemuxer::probe(get_io_service(), *stream_);
                if (demuxer) {
                    attach(*demuxer);
                    return true;
                } else {
                    ec = boost::asio::error::try_again;
                }
            }
            return false;
        }

        void SingleDemuxer::handle_async_open(
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
                        boost::bind(&SingleDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    media_.get_info(media_info_, ec);
                    media_.get_url(url_, ec);
                    if (!ec) {
                        util::stream::UrlSource * source = 
                            util::stream::UrlSource::create(get_io_service(), media_.get_protocol(), ec);
                        if (source) {
                            boost::system::error_code ec1;
                            source->set_non_block(true, ec1);
                            source_ = new ppbox::data::SingleSource(url_, *source);
                            source_->set_time_out(5000);
                            stream_ = new ppbox::data::SourceStream(*source_, 10 * 1024 * 1024, 10240);
                            // TODO:
                            open_state_ = demuxer_probe;
                            DemuxStatistic::open_beg_demux();
                            stream_->pause_stream();
                            stream_->seek(0, ec);
                            stream_->pause_stream();
                        }
                    }
                case demuxer_probe:
                    if (!ec && create_demuxer(ec)) {
                        open_state_ = demuxer_open;
                        CustomDemuxer::open(ec);
                        CustomDemuxer::reset(ec);
                    } else if (ec == boost::asio::error::try_again && stream_->out_position() < 1024 * 1024) {
                        stream_->async_prepare_some(0, 
                            boost::bind(&SingleDemuxer::handle_async_open, this, _1));
                        break;
                    }
                case demuxer_open:
                    if (!ec && !seek(seek_time_, ec)) { // 上面的reset可能已经有错误，所以判断ec
                        open_state_ = opened;
                        stream_->set_track_count(get_stream_count(ec));
                        media_info_.file_size = source_->total_size();
                        using ppbox::data::invalid_size;
                        if (media_info_.duration == invalid_size) {
                            boost::system::error_code ec;
                            CustomDemuxer::get_media_info(media_info_, ec);
                        }
                        if (media_info_.bitrate == 0 && media_info_.file_size != invalid_size && media_info_.duration != invalid_size) {
                            media_info_.bitrate = (boost::uint32_t)(media_info_.file_size * 8 * 1000 / media_info_.duration);
                        }
                        open_end();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block || (ec == file_stream_error 
                        && stream_->last_error() == boost::asio::error::would_block)) {
                            stream_->async_prepare_some(0, 
                                boost::bind(&SingleDemuxer::handle_async_open, this, _1));
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
            if (&time != &seek_time_ && open_state_ == opened) {
                DemuxStatistic::seek(!ec, time);
            }
            if (ec) {
                DemuxStatistic::last_error(ec);
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
                info = media_info_;
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

        boost::system::error_code SingleDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            stream_->prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                return ec;
            }
            assert(!seek_pending_);

            if (sample.memory) {
                stream_->putback(sample.memory);
                sample.memory = NULL;
            }

            sample.data.clear();
            CustomDemuxer::get_sample(sample, ec);
            if (ec == file_stream_error) {
                ec = stream_->last_error();
                assert(ec);
                if (ec == boost::asio::error::eof)
                    ec = end_of_stream;
                if (!ec) {
                    ec = boost::asio::error::would_block;
                }
            }
            if (!ec) {
                DemuxStatistic::play_on(sample.time);
                sample.memory = stream_->fetch(sample.itrack, *(std::vector<ppbox::data::DataBlock> *)sample.context, sample.data, ec);
                assert(!ec);
            } else {
                DemuxStatistic::last_error(ec);
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

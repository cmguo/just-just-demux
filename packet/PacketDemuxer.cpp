// PacketDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/packet/PacketDemuxer.h"
#include "ppbox/demux/packet/filter/SourceFilter.h"

#include <ppbox/data/packet/PacketMedia.h>
#include <ppbox/data/packet/PacketBuffer.h>
#include <ppbox/data/base/SourceBase.h>

using namespace ppbox::avformat;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/LogicError.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.PacketDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        PacketDemuxer::PacketDemuxer(
            boost::asio::io_service & io_svc, 
            ppbox::data::PacketMedia & media)
            : Demuxer(io_svc)
            , media_(media)
            , source_(media)
            , seek_time_(0)
            , seek_pending_(false)
            , open_state_(not_open)
        {
        }

        PacketDemuxer::~PacketDemuxer()
        {
            boost::system::error_code ec;
            close(ec);
            //if (source_) {
            //    ppbox::data::SourceBase * source = (ppbox::data::SourceBase *)&source_->source();
            //    ppbox::data::SourceBase::destroy(source);
            //    delete source_;
            //    source_ = NULL;
            //}
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
            cancel(ec);
            seek_time_ = 0;
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
                    DemuxStatistic::open_beg();
                    media_.async_open(
                        boost::bind(&PacketDemuxer::handle_async_open, this, _1));
                    break;
                case media_open:
                    open_state_ = demuxer_open;
                    media_.get_info(media_info_, ec);
                    media_.source().set_non_block(true, ec);
                    filters_.push_back(new SourceFilter(source_));
                    source_.pause_stream();
                case demuxer_open:
                    source_.pause_stream();
                    if (!ec && check_open(ec)) { // 上面的reset可能已经有错误，所以判断ec
                        open_state_ = open_finished;
                        on_open();
                        open_end();
                        source_.resume_stream();
                        response(ec);
                    } else if (ec == boost::asio::error::would_block) {
                        source_.async_prepare(
                            boost::bind(&PacketDemuxer::handle_async_open, this, _1));
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
            ppbox::data::MediaInfo & info,
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

        boost::uint64_t PacketDemuxer::check_seek(
            boost::system::error_code & ec)
        {
            //stream_->prepare_some(ec);
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
            source_.prepare(ec);
            return !ec;
        }

        bool PacketDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                //CustomDemuxer::get_stream_status(info, ec);
                return !ec;
            }
            return false;
        }

        bool PacketDemuxer::get_data_stat(
            DataStatistic & stat, 
            boost::system::error_code & ec) const
        {
            if (is_open(ec)) {
                //stat = *source_;
                return true;
            }
            return false;
        }

        boost::system::error_code PacketDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            source_.prepare_some(ec);
            if (seek_pending_ && seek(seek_time_, ec)) {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                }
                return ec;
            }
            assert(!seek_pending_);

            get_sample2(sample, ec);

            if (!ec) {
                DemuxStatistic::play_on(sample.time);
            } else {
                if (ec == boost::asio::error::would_block) {
                    DemuxStatistic::block_on();
                } else if (ec == boost::asio::error::eof) {
                    ec = error::no_more_sample;
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
                source_.putback(sample.memory);
                sample.memory = NULL;
            }
            ec.clear();
            return true;
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
            if (peek_samples_.empty()) {
                while (true) {
                    sample.data.clear();
                    filters_.last()->get_sample(sample, ec);
                    if (ec || (sample.flags & sample.config) == 0)
                        break;
                }
            } else {
                sample = peek_samples_.front();
                peek_samples_.pop_front();
                ec.clear();
            }
            if (!ec && sample.itrack < stream_infos_.size()) {
                Demuxer::adjust_timestamp(sample);
            }
        }

        bool PacketDemuxer::peek_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            sample.data.clear();
            filters_.last()->get_sample(sample, ec);
            if (!ec) {
                peek_samples_.push_back(sample);
                return true;
            }
            return false;
        }

        void PacketDemuxer::drop_sample()
        {
            peek_samples_.pop_back();
        }

    } // namespace demux
} // namespace ppbox

// RecordDemuxer.h

#include "ppbox/demux/Common.h"
#if 0
#include "ppbox/demux/RecordDemuxer.h"
using namespace ppbox::demux::error;

#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("RecordDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        void RecordDemuxer::set_pool_size(boost::uint32_t size)
        {
            pool_max_size_ = size;
        }

        void RecordDemuxer::add_stream(MediaInfo const & info)
        {
            media_infos_.push_back(info);
        }

        void RecordDemuxer::push(Frame const & frame)
        {
            boost::mutex::scoped_lock lock(mutex_);
            if (pool_.size() == pool_max_size_) {
                pool_.pop_front();
                LOG_S(Logger::kLevelEvent, "record empty is full");
            }
            pool_.push_back(frame);
        }

        void RecordDemuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            error_code ec;
            if (media_infos_.size() <2) {
                step_ = StepType::not_open;
                ec = error::not_open;
            } else {
                step_ = StepType::opened;
            }
            io_svc_.post(boost::bind(resp, ec));
        }

        bool RecordDemuxer::is_open(
            error_code & ec)
        {
            if (step_ == opened) {
                ec = error_code();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        error_code RecordDemuxer::cancel(
            error_code & ec)
        {
            ec = error_code();
            return ec;
        }

        error_code RecordDemuxer::pause(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code RecordDemuxer::resume(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code RecordDemuxer::close(
            error_code & ec)
        {
            ec = error_code();
            DemuxerStatistic::close();
            return ec;
        }

        size_t RecordDemuxer::get_media_count(
            error_code & ec)
        {
            if (media_infos_.size() < 2) {
                ec = error::not_open;
            } else {
                ec = error_code();
            }
            return media_infos_.size();
        }

        error_code RecordDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            error_code & ec)
        {
            assert(index < 2);
            if (media_infos_.size() < 2) {
                ec = error::not_open;
            } else {
                ec = error_code();
                info = media_infos_[index];
            }
            return ec;
        }

        boost::uint32_t RecordDemuxer::get_duration(
            error_code & ec)
        {
            ec = not_support;
            return 0;
        }

        error_code RecordDemuxer::seek(
            boost::uint32_t & time, 
            error_code & ec)
        {
            ec = not_support;
            if (0 ==time) {
                ec = error_code();
            }
            return ec;
        }


        boost::uint32_t RecordDemuxer::get_end_time(
            error_code & ec, 
            error_code & ec_buf)
        {
            boost::mutex::scoped_lock lock(mutex_);
            if (!pool_.empty()) {
                return (boost::uint32_t)(pool_.back().dts);
            } else {
                return cur_frame_.dts;
            }
        }

        boost::uint32_t RecordDemuxer::get_cur_time(
            error_code & ec)
        {
            ec = error_code();
            return cur_frame_.dts;
        }

        error_code RecordDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            boost::mutex::scoped_lock lock(mutex_);
            if (pool_.empty()) {
                ec = boost::asio::error::would_block;
                LOG_S(Logger::kLevelEvent, "record pool is empty");
            } else {
                ec = error_code();
                cur_frame_ = pool_.front();
                pool_.pop_front();
                sample.data.clear();
                sample.itrack = cur_frame_.itrack;
                sample.dts = cur_frame_.dts;
                sample.time = (sample.dts * 1000) / media_infos_[sample.itrack].time_scale;
                sample.ustime = sample.time * 1000;
                sample.cts_delta = boost::uint32_t(-1);
                sample.flags |= Sample::sync;
                sample.data.push_back(boost::asio::buffer(cur_frame_.data));
                sample.size = cur_frame_.data.size();
            }
            return ec;
        }

        error_code RecordDemuxer::set_non_block(
            bool non_block, 
            boost::system::error_code & ec)
        {
            ec = error_code();
            return ec;
        }

        error_code RecordDemuxer::set_time_out(
            boost::uint32_t time_out, 
            boost::system::error_code & ec)
        {
            ec = error_code();
            return ec;
        }

    } // namespace demux
} // namespace ppbox
#endif

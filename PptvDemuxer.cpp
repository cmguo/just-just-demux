// PptvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/PptvDemuxer.h"
#include "ppbox/demux/source/BufferList.h"
#include "ppbox/demux/source/SourceBase.h"

#include <framework/timer/Ticker.h>
#include <framework/logger/LoggerSection.h>
using namespace framework::logger;

#include <boost/system/error_code.hpp>
using namespace boost::system;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        PptvDemuxer::PptvDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            SourceBase * segmentbase)
            : BufferDemuxer(io_svc, buffer_size, prepare_size, segmentbase)
            , io_svc_(io_svc)
            , segments_(segmentbase)
        {
            ticker_ = new framework::timer::Ticker(1000);
        }

        PptvDemuxer::~PptvDemuxer()
        {
            delete ticker_;
            ticker_ = NULL;
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec)
                : ec_(ec)
                , returned_(false)
            {
            }

            void operator()(
                error_code const & ec)
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

            error_code & ec_;
            bool returned_;
            boost::mutex mutex_;
            boost::condition_variable cond_;
        };

        error_code PptvDemuxer::open(
            std::string const & name, 
            error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(name, boost::ref(resp));
            resp.wait();

            return ec;
        }

        void PptvDemuxer::tick_on()
        {
            if (ticker_->check()) {
                //LOG_S(Logger::kLevelDebug, "[tick_on]");

                update_stat();
            }
        }

        void PptvDemuxer::update_stat()
        {
            error_code ec;
            error_code ec_buf;
            boost::uint32_t buffer_time = get_buffer_time(ec, ec_buf);
            set_buf_time(buffer_time);
        }

        void PptvDemuxer::on_extern_error(
            boost::system::error_code const & ec)
        {
            //extern_error_ = ec;
        }

        size_t PptvDemuxer::get_segment_count(
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return 0;
        }

        boost::system::error_code PptvDemuxer::get_sample_buffered(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (state_ == buffering && play_position_ > seek_position_ + 30000) {
                boost::system::error_code ec_buf;
                boost::uint32_t time = get_buffer_time(ec, ec_buf);
                if (ec && ec != boost::asio::error::would_block) {
                } else {
                    //if (time < 2000 && ec_buf != boost::asio::error::eof) {
                    //    ec = ec_buf;
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

        boost::uint32_t PptvDemuxer::get_buffer_time(
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

        void PptvDemuxer::on_event(
            DemuxerEventType::Enum event_type,
            void const * const arg,
            boost::system::error_code & ec)
        {
            ec.clear();
            if (event_type == DemuxerEventType::play_on) {
                play_on(* (boost::uint32_t *) arg);
            } else if (event_type == DemuxerEventType::seek_segment) {
                demux_data().segment = * (size_t *)arg;
            } else if (event_type == DemuxerEventType::seek_statistic) {
                DemuxerStatistic::seek(* (boost::uint32_t *)arg, ec);
            }
        }

        boost::system::error_code PptvDemuxer::get_segment_info(
            SegmentInfo & info, 
            bool need_head_data, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        boost::system::error_code PptvDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        boost::system::error_code PptvDemuxer::set_max_dl_speed(
            boost::uint32_t speed, // KBps
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

    }
}

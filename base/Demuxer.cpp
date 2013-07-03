// Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/Demuxer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>
#include <boost/thread/condition_variable.hpp>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.Demuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        Demuxer::Demuxer(
            boost::asio::io_service & io_svc)
            : DemuxerBase(io_svc)
            , DemuxStatistic((DemuxerBase &)*this)
            , timestamp_(&default_timestamp_)
        {
        }

        Demuxer::~Demuxer()
        {
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

        boost::system::error_code Demuxer::open(
            boost::system::error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(boost::ref(resp));
            resp.wait();
            return ec;
        }

        void Demuxer::async_open(
            open_response_type const & resp)
        {
            boost::system::error_code ec;
            open(ec);
            get_io_service().post(boost::bind(resp, ec));
        }

        boost::system::error_code Demuxer::cancel(
            boost::system::error_code & ec)
        {
            return ec = framework::system::logic_error::not_supported;
        }

        boost::system::error_code Demuxer::close(
            boost::system::error_code & ec)
        {
            return ec;
        }

        boost::system::error_code Demuxer::get_media_info(
            MediaInfo & info, 
            boost::system::error_code & ec) const
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        bool Demuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return false;
        }

        bool Demuxer::get_data_stat(
            SourceStatisticData & stat, 
            boost::system::error_code & ec) const
        {
            ec = framework::system::logic_error::not_supported;
            return false;
        }

        boost::system::error_code Demuxer::reset(
            boost::system::error_code & ec)
        {
            boost::uint64_t time  = 0;
            seek(time, ec);
            return ec;
        }

        boost::system::error_code Demuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        boost::uint64_t Demuxer::check_seek(
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ppbox::data::invalid_size;
        }

        boost::system::error_code Demuxer::pause(
            boost::system::error_code & ec)
        {
            return ec = framework::system::logic_error::not_supported;
        }

        boost::system::error_code Demuxer::resume(
            boost::system::error_code & ec)
        {
            return ec = framework::system::logic_error::not_supported;
        }

        bool Demuxer::fill_data(
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return false;
        }

        void Demuxer::on_open()
        {
            if (timestamp_ == &default_timestamp_) {
                timestamp_->begin(*this);
            }
        }

        void Demuxer::on_close()
        {
            if (timestamp_ == &default_timestamp_) {
                timestamp_->end(*this);
            }
        }

    } // namespace demux
} // namespace ppbox

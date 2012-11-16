// Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/Demuxer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.Demuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        Demuxer::Demuxer(
            boost::asio::io_service & io_svc, 
            streambuffer_t & buf)
            : DemuxerBase(io_svc)
            , buf_(buf)
            , helper_(&default_helper_)
        {
        }

        Demuxer::~Demuxer()
        {
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

        boost::system::error_code Demuxer::get_media_info(
            MediaInfo & info, 
            boost::system::error_code & ec) const
        {
            info.duration = get_duration(ec);
            return ec;
        }

        boost::system::error_code Demuxer::get_play_info(
            PlayInfo & info, 
            boost::system::error_code & ec) const
        {
            return ec;
        }

        bool Demuxer::get_data_stat(
            DataStatistic & stat, 
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
            boost::uint64_t delta  = 0;
            boost::uint64_t offset = seek(time, delta, ec);
            if (ec) {
                return ec;
            }
            if (buf_.pubseekpos(offset) != std::streampos(offset)) {
                ec = error::file_stream_error;
                return ec;
            }
            return ec;
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

        void Demuxer::demux_begin(
            TimestampHelper & helper)
        {
            helper_ = &helper;
            boost::system::error_code ec;
            size_t n = get_stream_count(ec);
            if (n == 0)
                return;
            std::vector<boost::uint64_t> scale;
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                get_stream_info(i, info, ec);
                scale.push_back(info.time_scale);
                dts.push_back(info.start_time);
            }
            helper_->set_scale(scale);
            helper_->begin(dts);
        }

        void Demuxer::demux_end()
        {
            boost::system::error_code ec;
            size_t n = get_stream_count(ec);
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                get_stream_info(i, info, ec);
                dts.push_back(info.start_time + info.duration);
            }
            helper_->end(dts);
            helper_ = &default_helper_;
        }

        void Demuxer::on_open()
        {
            demux_begin(*helper_);
        }

    } // namespace demux
} // namespace ppbox

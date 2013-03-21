// BasicDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/BasicDemuxer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.BasicDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        BasicDemuxer::BasicDemuxer(
            boost::asio::io_service & io_svc, 
            streambuffer_t & buf)
            : Demuxer(io_svc)
            , buf_(buf)
        {
        }

        BasicDemuxer::~BasicDemuxer()
        {
        }

        boost::system::error_code BasicDemuxer::get_media_info(
            MediaInfo & info, 
            boost::system::error_code & ec) const
        {
            info.duration = get_duration(ec);
            return ec;
        }

        bool BasicDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            using ppbox::data::invalid_size;

            info.byte_range.beg = 0;
            info.byte_range.end = invalid_size;
            info.byte_range.pos = buf_.pubseekoff(0, std::ios::cur, std::ios::in);
            info.byte_range.buf = buf_.pubseekoff(0, std::ios::end, std::ios::in);
            buf_.pubseekoff(info.byte_range.pos, std::ios::beg, std::ios::in);

            info.time_range.beg = 0;
            info.time_range.end = get_duration(ec);
            info.time_range.pos = get_cur_time(ec);
            info.time_range.buf = get_end_time(ec);

            return !ec;
        }

        boost::system::error_code BasicDemuxer::reset(
            boost::system::error_code & ec)
        {
            boost::uint64_t time  = 0;
            seek(time, ec);
            return ec;
        }

        boost::system::error_code BasicDemuxer::seek(
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

    } // namespace demux
} // namespace ppbox

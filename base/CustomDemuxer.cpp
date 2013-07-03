// CustomDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/CustomDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        CustomDemuxer::CustomDemuxer(
            DemuxerBase & demuxer)
            : Demuxer(demuxer.get_io_service())
            , demuxer_(demuxer)
        {
        }

        CustomDemuxer::~CustomDemuxer()
        {
        }

        boost::system::error_code CustomDemuxer::open (
            boost::system::error_code & ec)
        {
            return demuxer_.open(ec);
        }

        void CustomDemuxer::async_open(
            open_response_type const & resp)
        {
            return demuxer_.async_open(resp);
        }

        bool CustomDemuxer::is_open(
            boost::system::error_code & ec)
        {
            return demuxer_.is_open(ec);
        }

        boost::system::error_code CustomDemuxer::cancel(
            boost::system::error_code & ec)
        {
            return demuxer_.cancel(ec);
        }

        boost::system::error_code CustomDemuxer::close(
            boost::system::error_code & ec)
        {
            return demuxer_.close(ec);
        }

        boost::system::error_code CustomDemuxer::get_media_info(
            ppbox::data::MediaInfo & info, 
            boost::system::error_code & ec) const
        {
            return demuxer_.get_media_info(info, ec);
        }

        size_t CustomDemuxer::get_stream_count(
            boost::system::error_code & ec) const
        {
            return demuxer_.get_stream_count(ec);
        }

        boost::system::error_code CustomDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            boost::system::error_code & ec) const
        {
            return demuxer_.get_stream_info(index, info, ec);
        }

        bool CustomDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec) const
        {
            return demuxer_.get_stream_status(info, ec);
        }

        bool CustomDemuxer::get_data_stat(
            SourceStatisticData & stat, 
            boost::system::error_code & ec) const
        {
            return demuxer_.get_data_stat(stat, ec);
        }

        boost::system::error_code CustomDemuxer::reset(
            boost::system::error_code & ec)
        {
            return demuxer_.reset(ec);
        }

        boost::system::error_code CustomDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            return demuxer_.seek(time, ec);
        }

        boost::system::error_code CustomDemuxer::pause(
            boost::system::error_code & ec)
        {
            return demuxer_.pause(ec);
        }

        boost::system::error_code CustomDemuxer::resume(
            boost::system::error_code & ec)
        {
            return demuxer_.resume(ec);
        }

        boost::system::error_code CustomDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            return demuxer_.get_sample(sample, ec);
        }

    } // namespace demux
} // namespace ppbox

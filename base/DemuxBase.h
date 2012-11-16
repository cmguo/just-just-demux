// DemuxBase.h

#ifndef _PPBOX_DEMUX_DEMUX_BASE_H_
#define _PPBOX_DEMUX_DEMUX_BASE_H_

#include <ppbox/data/base/MediaInfo.h>
#include <ppbox/data/base/PlayInfo.h>
#include <ppbox/data/base/DataStatistic.h>

#include <ppbox/avformat/Format.h>

#include <boost/function.hpp>

namespace ppbox
{
    namespace demux
    {

        using ppbox::data::MediaInfo;
        using ppbox::data::PlayInfo;
        using ppbox::data::DataStatistic;

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        typedef boost::function<void (
            boost::system::error_code const &)
        > open_response_type;

        class DemuxerBase;

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUX_BASE_H_

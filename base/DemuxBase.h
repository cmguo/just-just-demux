// DemuxBase.h

#ifndef _PPBOX_DEMUX_DEMUX_BASE_H_
#define _PPBOX_DEMUX_DEMUX_BASE_H_

#include <ppbox/data/base/DataBase.h>

#include <ppbox/avbase/StreamType.h>
#include <ppbox/avbase/StreamInfo.h>
#include <ppbox/avbase/Sample.h>
#include <ppbox/avbase/StreamStatus.h>

namespace ppbox
{
    namespace demux
    {

        using ppbox::data::DataStat;

        using ppbox::avbase::StreamType;
        using ppbox::avbase::StreamInfo;
        using ppbox::avbase::Sample;
        using ppbox::avbase::StreamStatus;
        using ppbox::avbase::MediaInfo;

        class DemuxerBase;

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUX_BASE_H_

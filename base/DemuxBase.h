// DemuxBase.h

#ifndef _PPBOX_DEMUX_DEMUX_BASE_H_
#define _PPBOX_DEMUX_DEMUX_BASE_H_

#include <ppbox/data/base/DataBase.h>

#include <ppbox/avbase/StreamType.h>
#include <ppbox/avbase/StreamInfo.h>
#include <ppbox/avbase/Sample.h>

namespace ppbox
{
    namespace demux
    {

        using ppbox::data::MediaInfo;
        using ppbox::data::StreamStatus;
        using ppbox::data::SourceStatisticData;

        using ppbox::avbase::StreamType;
        using ppbox::avbase::StreamInfo;
        using ppbox::avbase::Sample;

        class DemuxerBase;

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUX_BASE_H_

// DemuxBase.h

#ifndef _JUST_DEMUX_DEMUX_BASE_H_
#define _JUST_DEMUX_DEMUX_BASE_H_

#include <just/data/base/DataBase.h>

#include <just/avbase/StreamType.h>
#include <just/avbase/StreamInfo.h>
#include <just/avbase/Sample.h>
#include <just/avbase/StreamStatus.h>

namespace just
{
    namespace demux
    {

        using just::data::DataStat;

        using just::avbase::StreamType;
        using just::avbase::StreamInfo;
        using just::avbase::Sample;
        using just::avbase::StreamStatus;
        using just::avbase::MediaInfo;

        class DemuxerBase;

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_DEMUX_BASE_H_

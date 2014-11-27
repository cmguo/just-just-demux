// DemuxStatistic.cpp

#include "just/demux/Common.h"
#include "just/demux/base/DemuxStatistic.h"
#include "just/demux/base/Demuxer.h"

namespace just
{
    namespace demux
    {

        DemuxStatistic::DemuxStatistic(
            DemuxerBase & demuxer)
            : demuxer_(demuxer)
        {
        }

        void DemuxStatistic::update_stat(
            boost::system::error_code & ec)
        {
            demuxer_.get_stream_status(*this, ec);
        }

    }
}

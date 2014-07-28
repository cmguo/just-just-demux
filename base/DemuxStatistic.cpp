// DemuxStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxStatistic.h"
#include "ppbox/demux/base/Demuxer.h"

namespace ppbox
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

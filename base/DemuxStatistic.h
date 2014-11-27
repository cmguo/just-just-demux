// DemuxStatistic.h

#ifndef _JUST_DEMUX_BASE_DEMUX_STATISTIC_H_
#define _JUST_DEMUX_BASE_DEMUX_STATISTIC_H_

#include "just/demux/base/DemuxBase.h"

#include <just/avbase/StreamStatistic.h>

namespace just
{
    namespace demux
    {

        class DemuxStatistic
            : public just::avbase::StreamStatistic
        {
        protected:
            DemuxStatistic(
                DemuxerBase & demuxer);

        private:
            virtual void update_stat(
                boost::system::error_code & ec);

        private:
            DemuxerBase & demuxer_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASE_DEMUX_STATISTIC_H_

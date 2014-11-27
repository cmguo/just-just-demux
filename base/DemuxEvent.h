// DemuxEvent.h

#ifndef _JUST_DEMUX_BASE_DEMUX_EVENT_H_
#define _JUST_DEMUX_BASE_DEMUX_EVENT_H_

#include <util/event/Event.h>

namespace just
{
    namespace demux
    {

        class DemuxStatistic;

        class DemuxStatisticEvent
            : public util::event::Event
        {
        public:
            DemuxStatistic const & stat;

            DemuxStatisticEvent(
                DemuxStatistic const & stat)
                : stat(stat)
            {
            }
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASE_DEMUXER_STATISTIC_H_

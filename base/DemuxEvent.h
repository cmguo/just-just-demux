// DemuxEvent.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_EVENT_H_
#define _PPBOX_DEMUX_BASE_DEMUX_EVENT_H_

#include <util/event/Event.h>

namespace ppbox
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
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUXER_STATISTIC_H_

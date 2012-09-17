// DemuxEvent.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_EVENT_H_
#define _PPBOX_DEMUX_BASE_DEMUX_EVENT_H_

#include <util/event/Event.h>

namespace ppbox
{
    namespace demux
    {

        class DemuxStatistic;

        class StatusChangeEvent
            : public util::event::EventBase<StatusChangeEvent>
        {
        public:
            DemuxStatistic const & stat;

            StatusChangeEvent(
                DemuxStatistic const & stat)
                : stat(stat)
            {
            }
        };

        // 定期发出的缓存状态

        class BufferingEvent
            : public util::event::EventBase<BufferingEvent>
        {
        public:
            DemuxStatistic const & stat;

            BufferingEvent(
                DemuxStatistic const & stat)
                : stat(stat)
            {
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUXER_STATISTIC_H_

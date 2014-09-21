// DemuxStatistic.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

#include "ppbox/demux/base/DemuxBase.h"

#include <ppbox/avbase/StreamStatistic.h>

namespace ppbox
{
    namespace demux
    {

        class DemuxStatistic
            : public ppbox::avbase::StreamStatistic
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
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

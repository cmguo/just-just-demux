// TimestampFilter.h

#ifndef _PPBOX_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_
#define _PPBOX_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_

#include "ppbox/demux/packet/Filter.h"
#include "ppbox/demux/base/TimestampHelper.h"

namespace ppbox
{
    namespace demux
    {

        class TimestampFilter
            : public Filter
        {
        public:
            TimestampFilter(
                TimestampHelper & helper);

            ~TimestampFilter();

        public:
            virtual bool get_sample(
                Sample & sample,
                boost::system::error_code & ec);

            virtual bool get_next_sample(
                Sample & sample,
                boost::system::error_code & ec);

            virtual bool get_last_sample(
                Sample & sample,
                boost::system::error_code & ec);

        private:
            TimestampHelper & helper_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_

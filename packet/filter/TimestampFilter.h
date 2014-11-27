// TimestampFilter.h

#ifndef _JUST_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_
#define _JUST_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_

#include "just/demux/packet/Filter.h"
#include "just/demux/base/TimestampHelper.h"

namespace just
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
} // namespace just

#endif // _JUST_DEMUX_PACKET_FILTER_TIMESTAMP_FILTER_H_

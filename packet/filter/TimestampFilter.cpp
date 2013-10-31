// TimestampFilter.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/packet/filter/TimestampFilter.h"

namespace ppbox
{
    namespace demux
    {

        TimestampFilter::TimestampFilter(
            TimestampHelper & helper)
            : helper_(helper)
        {
        }

        TimestampFilter::~TimestampFilter()
        {
        }

        bool TimestampFilter::get_sample(
            Sample & sample,
            boost::system::error_code & ec)
        {
            if (!Filter::get_sample(sample, ec)) {
                return false;
            }

            if ((sample.flags & sample.f_config) == 0)
                helper_.adjust(sample);

            return true;
        }

        bool TimestampFilter::get_next_sample(
            Sample & sample,
            boost::system::error_code & ec)
        {
            if (!Filter::get_next_sample(sample, ec))
                return false;

            helper_.const_adjust(sample);

            return true;
        }

        bool TimestampFilter::get_last_sample(
            Sample & sample,
            boost::system::error_code & ec)
        {
            if (!Filter::get_last_sample(sample, ec))
                return false;

            helper_.const_adjust(sample);

            return true;
        }

    } // namespace demux
} // namespace ppbox

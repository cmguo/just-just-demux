// SortFilter.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/packet/filter/SortFilter.h"

namespace ppbox
{
    namespace demux
    {

        SortFilter::SortFilter(
            boost::uint32_t stream_count)
            : eof_(false)
        {
            samples_.resize(stream_count);
        }

        SortFilter::~SortFilter()
        {
        }

        inline bool SortFilter::less_sample_queue::operator()(
            SampleQueue * l, 
            SampleQueue * r)
        {
            return l->front().time < r->front().time;
        }

        bool SortFilter::get_sample(
            Sample & sample,
            boost::system::error_code & ec)
        {
            if (eof_ && orders_.empty()) {
                ec = error::no_more_sample;
                return false;
            }

            while (!eof_ && orders_.size() < samples_.size()) {
                if (Filter::get_sample(sample, ec)) {
                    SampleQueue & queue = samples_[sample.itrack];
                    if (queue.empty()) {
                        queue.push_back(sample);
                        sample.data.clear();
                        orders_.insert(&queue);
                    } else {
                        queue.push_back(sample);
                        sample.data.clear();
                    }
                } else if (ec == error::no_more_sample) {
                    eof_ = true;
                    if (orders_.empty()) {
                        ec = error::no_more_sample;
                        return false;
                    }
                } else {
                    return false;
                }
            }

            SampleQueue & queue = **orders_.begin();
            orders_.erase(orders_.begin());
            sample = queue.front();
            queue.pop_front();
            if (!queue.empty()) {
                orders_.insert(&queue);
            }

            ec.clear();
            return true;
        }

        bool SortFilter::before_seek(
            Sample & sample,
            boost::system::error_code & ec)
        {
            for (size_t i = 0; i < samples_.size(); ++i) {
                SampleQueue & queue = samples_[i];
                for (size_t j = 0; j < queue.size(); ++j) {
                    sample.append(queue[j]);
                }
                queue.clear();
            }
            orders_.clear();
            return Filter::before_seek(sample, ec);
        }

    } // namespace demux
} // namespace ppbox

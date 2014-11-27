// SortFilter.cpp

#include "just/demux/Common.h"
#include "just/demux/packet/filter/SortFilter.h"

using namespace just::avformat::error;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

namespace just
{
    namespace demux
    {

        FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.SortFilter", framework::logger::Debug);

        SortFilter::SortFilter(
            boost::uint32_t stream_count)
            : eof_(false)
        {
            sample_queues_.resize(stream_count);
        }

        SortFilter::~SortFilter()
        {
        }

        inline bool SortFilter::less_sample_queue::operator()(
            SampleQueue * l, 
            SampleQueue * r)
        {
            Sample & sl = l->front();
            Sample & sr = r->front();
            return sl.time < sr.time || (sl.time == sr.time && sl.itrack < sr.itrack);
        }

        bool SortFilter::get_sample(
            Sample & sample,
            boost::system::error_code & ec)
        {
            if (eof_ && orders_.empty()) {
                ec = end_of_stream;
                return false;
            }

            while (!eof_ && orders_.size() < sample_queues_.size()) {
                if (Filter::get_sample(sample, ec)) {
                    LOG_TRACE("[get_sample] in itrack: " << sample.itrack << " time: " << sample.time << " dts: " << sample.dts);
                    SampleQueue & queue = sample_queues_[sample.itrack];
                    if (queue.empty()) {
                        queue.push_back(sample);
                        sample.data.clear();
                        orders_.insert(&queue);
                    } else {
                        queue.push_back(sample);
                        sample.data.clear();
                    }
                } else if (ec == end_of_stream) {
                    eof_ = true;
                    if (orders_.empty()) {
                        ec = end_of_stream;
                        return false;
                    }
                } else {
                    return false;
                }
            }

            SampleQueue & queue = **orders_.begin();
            orders_.erase(orders_.begin());
            sample = queue.front();
            LOG_TRACE("[get_sample] out itrack: " << sample.itrack << " time: " << sample.time << " dts: " << sample.dts);
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
            for (size_t i = 0; i < sample_queues_.size(); ++i) {
                SampleQueue & queue = sample_queues_[i];
                for (size_t j = 0; j < queue.size(); ++j) {
                    sample.append(queue[j]);
                }
                queue.clear();
            }
            orders_.clear();
            return Filter::before_seek(sample, ec);
        }

    } // namespace demux
} // namespace just

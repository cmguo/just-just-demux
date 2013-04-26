// SortFilter.h

#ifndef _PPBOX_DEMUX_PACKET_FILTER_SORT_FILTER_H_
#define _PPBOX_DEMUX_PACKET_FILTER_SORT_FILTER_H_

#include "ppbox/demux/packet/Filter.h"

namespace ppbox
{
    namespace demux
    {

        class SortFilter
            : public Filter
        {
        public:
            SortFilter(
                boost::uint32_t stream_count);

            ~SortFilter();

        public:
            virtual bool get_sample(
                Sample & sample,
                boost::system::error_code & ec);

            virtual bool before_seek(
                Sample & sample,
                boost::system::error_code & ec);

        private:
            typedef std::deque<Sample> SampleQueue;
            struct less_sample_queue
            {
                bool operator()(
                    SampleQueue * l, 
                    SampleQueue * r);
            };

        private:
            std::vector<SampleQueue> samples_;
            std::set<SampleQueue *, less_sample_queue> orders_; 
            bool eof_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PACKET_FILTER_SORT_FILTER_H_

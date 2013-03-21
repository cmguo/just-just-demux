// SourceFilter.h

#ifndef _PPBOX_DEMUX_PACKET_FILTER_SOURCE_FILTER_H_
#define _PPBOX_DEMUX_PACKET_FILTER_SOURCE_FILTER_H_

#include "ppbox/demux/packet/Filter.h"

#include <ppbox/data/packet/PacketSource.h>

namespace ppbox
{
    namespace demux
    {

        class SourceFilter
            : public Filter
        {
        public:
            SourceFilter(
                ppbox::data::PacketSource & source)
                : source_(source)
            {
            }

            ~SourceFilter()
            {
            }

        public:
            virtual bool get_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                sample.context = source_.fetch(sample.size, sample.data, ec);
                sample.context = &source_;
                return !ec;
            }

            virtual void before_seek(
                boost::uint64_t time)
            {
            }

        private:
            ppbox::data::PacketSource & source_;
        };

    } // namespace mux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PACKET_FILTER_SOURCE_FILTER_H_

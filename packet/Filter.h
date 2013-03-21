// Filter.h

#ifndef _PPBOX_DEMUX_PACKET_FILTER_H_
#define _PPBOX_DEMUX_PACKET_FILTER_H_

#include "ppbox/demux/base/DemuxBase.h"

#include <framework/container/List.h>

namespace ppbox
{
    namespace demux
    {

        class Filter
            : public framework::container::ListHook<Filter>::type
        {
        public:
            virtual ~Filter()
            {
            }

        public:
            virtual bool get_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                return prev()->get_sample(sample, ec);
            }

            virtual void before_seek(
                boost::uint64_t time)
            {
                prev()->before_seek(time);
            }

        protected:
            void detach_self()
            {
                unlink();
            }
        };

    } // namespace mux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PACKET_FILTER_H_
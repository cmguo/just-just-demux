// Filter.h

#ifndef _JUST_DEMUX_PACKET_FILTER_H_
#define _JUST_DEMUX_PACKET_FILTER_H_

#include "just/demux/base/DemuxBase.h"

#include <framework/container/List.h>

namespace just
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

            virtual bool get_next_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                return prev()->get_next_sample(sample, ec);
            }

            virtual bool get_last_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                return prev()->get_last_sample(sample, ec);
            }

            virtual bool before_seek(
                Sample & sample,
                boost::system::error_code & ec)
            {
                return prev()->before_seek(sample, ec);
            }

        protected:
            void detach_self()
            {
                unlink();
            }
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_PACKET_FILTER_H_
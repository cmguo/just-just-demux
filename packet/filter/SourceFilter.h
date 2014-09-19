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
                sample.data.clear();
                sample.append(source_.fetch(sample.size, sample.data, ec));
                sample.context = &source_;
                return !ec;
            }

            virtual bool get_next_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                source_.peek_next(sample.size, sample.data, ec);
                sample.context = &source_;
                return !ec;
            }

            virtual bool get_last_sample(
                Sample & sample,
                boost::system::error_code & ec)
            {
                source_.peek_last(sample.size, sample.data, ec);
                sample.context = &source_;
                return !ec;
            }

            virtual bool before_seek(
                Sample & sample,
                boost::system::error_code & ec)
            {
                ec.clear();
                if (sample.memory) {
                    source_.putback(sample.memory);
                    sample.memory = NULL;
                }
                return true;
            }

        private:
            ppbox::data::PacketSource & source_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PACKET_FILTER_SOURCE_FILTER_H_

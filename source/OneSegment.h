// OneSegment.h

#ifndef _PPBOX_DEMUX_ONE_SEGMENT_H_
#define _PPBOX_DEMUX_ONE_SEGMENT_H_

#include "ppbox/demux/base/SourceBase.h"

namespace ppbox
{
    namespace demux
    {

        class OneSegment
        {
        public:
            OneSegment()
            {
            }

            ~OneSegment()
            {
            }

        public:
            virtual void set_name(
                std::string const & file) = 0;
        };

        template <typename Source>
        class OneSegmentT
            : public SourceBase
            , public OneSegment
        {
        public:
            OneSegmentT(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type)
                : SourceBase(io_svc)
                , demuxer_type_(demuxer_type)
            {
            }

        public:
            virtual DemuxerType::Enum demuxer_type()
            {
                return demuxer_type_;
            }

            virtual size_t segment_count() const
            {
                return 1;
            }

        private:
            DemuxerType::Enum demuxer_type_;
        };

    } // namespace demux
} // namespace ppbox

#endif
        }

    } // namespace demux
} // namespace ppbox

#endif

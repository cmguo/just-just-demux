// DemuxerInfo.h

#ifndef _JUST_DEMUX_SEGMENT_DEMUXER_INFO_H_
#define _JUST_DEMUX_SEGMENT_DEMUXER_INFO_H_

#include "just/demux/basic/BasicDemuxer.h"
#include "just/data/segment/SegmentStream.h"
#include "just/data/segment/SegmentBuffer.h"

namespace just
{
    namespace demux
    {

        struct DemuxerInfo
        {
            boost::uint32_t nref;
            just::data::SegmentPosition segment;
            just::data::SegmentStream stream;
            BasicDemuxer * demuxer;

            DemuxerInfo(
                just::data::SegmentBuffer & buffer)
                : nref(1)
                , stream(buffer, false)
                , demuxer(NULL)
            {
            }

            void attach()
            {
                ++nref;
            }

            bool detach()
            {
                assert(nref > 0);
                return --nref == 0;
            }
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_SEGMENT_DEMUXER_INFO_H_

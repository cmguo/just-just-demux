// DemuxerInfo.h

#ifndef _PPBOX_DEMUX_SEGMENT_DEMUXER_INFO_H_
#define _PPBOX_DEMUX_SEGMENT_DEMUXER_INFO_H_

#include "ppbox/demux/base/Demuxer.h"
#include "ppbox/data/segment/SegmentStream.h"
#include "ppbox/data/segment/SegmentBuffer.h"

namespace ppbox
{
    namespace demux
    {

        struct DemuxerInfo
        {
            boost::uint32_t nref;
            ppbox::data::SegmentPosition segment;
            ppbox::data::SegmentStream stream;
            Demuxer * demuxer;

            DemuxerInfo(
                ppbox::data::SegmentBuffer & buffer)
                : nref(1)
                , stream(buffer)
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
} // namespace ppbox

#endif // _PPBOX_DEMUX_SEGMENT_DEMUXER_INFO_H_

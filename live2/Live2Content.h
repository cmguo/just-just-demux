// Live2Content.h

#ifndef _PPBOX_DEMUX_LIVE2_SOURCE_H_
#define _PPBOX_DEMUX_LIVE2_SOURCE_H_

#include "ppbox/demux/base/Content.h"

namespace ppbox
{
    namespace demux
    {
        class Live2Content
            : public Content
        {
        public:
            Live2Content(
                boost::asio::io_service & io_svc,
                ppbox::common::SegmentBase * pSegment,
                ppbox::common::SourceBase * pSource);

            virtual ~Live2Content();


            virtual DemuxerType::Enum demuxer_type() const;


            virtual boost::system::error_code reset(
                SegmentPositionEx & segment);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // ОўГо
                SegmentPositionEx & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);
// 
//             bool next_segment(
//                 SegmentPositionEx & segment);

        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SOURCE__H_

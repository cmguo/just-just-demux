// VodSource.h

#ifndef _PPBOX_DEMUX_VOD_SOURCE_H_
#define _PPBOX_DEMUX_VOD_SOURCE_H_

#include "ppbox/demux/base/SourceBase.h"

namespace ppbox
{
    namespace demux
    {
        class VodSource
            : public SourceBase
        {
        public:
            VodSource(
                boost::asio::io_service & io_svc,
                ppbox::cdn::SegmentBase * pSegment,
                ppbox::demux::Source * pSource);

            virtual ~VodSource();

            virtual DemuxerType::Enum demuxer_type() const;

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // ОўГо
                SegmentPositionEx & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code reset(
                SegmentPositionEx & segment);
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SOURCE__H_

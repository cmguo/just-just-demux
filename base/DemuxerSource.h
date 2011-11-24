// Source.h

#ifndef _PPBOX_DEMUX_SOURCE_H_
#define _PPBOX_DEMUX_SOURCE_H_

#include "ppbox/demux/source/SourceBase.h"

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;
        class ByteStream;

        class BufferDemuxer;

        struct DemuxerSegment
        {
            boost::uint32_t duration;   // 分段时长（毫秒）
            boost::uint32_t duration_offset;    // 相对起始的时长起点，（毫秒）
            boost::uint64_t duration_offset_us; // 同上，（微秒）
            boost::uint64_t head_length;
        };

        typedef boost::intrusive_ptr<
            BytesStream> StreamPointer;

        typedef boost::intrusive_ptr<
            DemuxerBase> DemuxerPointer;

        class DemuxerSource
            : public SourceBase
        {
        public:
            virtual DemuxerSegment demuxer_segment(
                size_t segment) const
            {
                
            }

            void time_seek(
                boost::uint64_t time, 
                DemuxerSegment & , 
                SegmentPosition & );

        private:
            friend class BufferDemuxer;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_H_

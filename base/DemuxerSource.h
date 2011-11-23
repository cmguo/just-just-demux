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

        class Source
            : public SourceBase
        {
        public:
            DemuxerType::Enum demuxer_type;
            boost::uint32_t duration;   // 分段时长（毫秒）
            boost::uint32_t duration_offset;    // 相对起始的时长起点，（毫秒）
            boost::uint64_t duration_offset_us; // 同上，（微秒）

        private:
            typedef boost::intrusive_ptr<
                BytesStream> StreamPointer;

            typedef boost::intrusive_ptr<
                DemuxerBase> DemuxerPointer;

            friend class BufferDemuxer;

            StreamPointer insert_stream_;
            DemuxerPointer insert_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_H_

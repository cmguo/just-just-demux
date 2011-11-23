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

        class Source
            : public SourceBase
        {
        public:
            typedef boost::intrusive_ptr<
                BytesStream> StreamPointer;
            typedef boost::intrusive_ptr<
                DemuxerBase> DemuxerPointer;

            StreamPointer insert_stream_;
            DemuxerPointer insert_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_H_

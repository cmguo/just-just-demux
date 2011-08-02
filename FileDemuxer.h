// FileDemuxer.h

#ifndef _PPBOX_DEMUX_FILE_DEMUXER_H_
#define _PPBOX_DEMUX_FILE_DEMUXER_H_

#include "ppbox/demux/CommonDemuxer.h"
#include "ppbox/demux/mp4/Mp4BufferDemuxer.h"
#include "ppbox/demux/asf/AsfBufferDemuxer.h"
#include "ppbox/demux/flv/FlvBufferDemuxer.h"
#include "ppbox/demux/source/FileOneSegment.h"

namespace ppbox
{
    namespace demux
    {

        typedef CommonDemuxer<FileOneSegment, Mp4BufferDemuxer<FileOneSegment> > Mp4FileDemuxer;

        typedef CommonDemuxer<FileOneSegment, AsfBufferDemuxer<FileOneSegment> > AsfFileDemuxer;

        typedef CommonDemuxer<FileOneSegment, FlvBufferDemuxer<FileOneSegment> > FlvFileDemuxer;

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FILE_DEMUXER_H_

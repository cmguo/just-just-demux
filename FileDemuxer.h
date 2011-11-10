// FileDemuxer.h

#ifndef _PPBOX_DEMUX_FILE_DEMUXER_H_
#define _PPBOX_DEMUX_FILE_DEMUXER_H_

#include "ppbox/demux/CommonDemuxer.h"
#include "ppbox/demux/source/FileOneSegment.h"

namespace ppbox
{
    namespace demux
    {

        typedef CommonDemuxer<FileOneSegment> FileOneDemuxer;

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FILE_DEMUXER_H_

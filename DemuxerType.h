// DemuxerType.h

#ifndef _PPBOX_DEMUX_DEMUXER_TYPE_H_
#define _PPBOX_DEMUX_DEMUXER_TYPE_H_

namespace ppbox
{
    namespace demux
    {

        struct DemuxerType
        {
            enum Enum
            {
                none, 
                vod, 
                live, 
                live2, 
                mp4, 
                asf, 
                flv, 
            };
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_TYPE_H_

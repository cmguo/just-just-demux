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
                mp4, 
                asf, 
                flv, 
                ts, 
            };
        };

        class DemuxerBase;
        class BytesStream;

        DemuxerBase * create_demuxer_base(
            DemuxerType::Enum type,
            BytesStream & stream);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_TYPE_H_

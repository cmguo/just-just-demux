// PptvDemuxerType.h

#ifndef _PPBOX_DEMUX_SOURCE_SOURCE_TYPE_H_
#define _PPBOX_DEMUX_SOURCE_SOURCE_TYPE_H_

namespace ppbox
{
    namespace demux
    {

        struct SourceType
        {
            enum Enum
            {
                none, 
                http, 
                file, 
                pipe, 
            };
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_SOURCE_TYPE_H_

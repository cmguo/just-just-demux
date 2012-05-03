// SourceType.h

#ifndef _PPBOX_DEMUX_SOURCE_TYPE_H_
#define _PPBOX_DEMUX_SOURCE_TYPE_H_

namespace ppbox
{
    namespace demux
    {

        struct SourcePrefix
        {
            enum Enum
            {
                none, 
                vod, 
                live, 
                live2, 
                file_mp4, 
                file_asf, 
                file_flv, 
                http_mp4, 
                http_asf, 
                http_flv, 
                desc_mp4, 
                desc_asf, 
                desc_flv, 
                record,
            };
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_TYPE_H_

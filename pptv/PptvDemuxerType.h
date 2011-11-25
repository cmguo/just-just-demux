// PptvDemuxerType.h

#ifndef _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_TYPE_H_
#define _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_TYPE_H_

namespace ppbox
{
    namespace demux
    {

        struct PptvDemuxerType
        {
            enum Enum
            {
                none, 
                vod, 
                live, 
                live2, 
                test,
            };
        };

        class PptvDemuxer;

        PptvDemuxer * pptv_create_demuxer(
            std::string const & proto);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_TYPE_H_

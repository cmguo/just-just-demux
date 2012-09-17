// PptvDemuxerType.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxType.h"
#include "ppbox/demux/base/BytesStream.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/asf/AsfDemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"

namespace ppbox
{
    namespace demux
    {

        DemuxerBase * create_demuxer_base(
            DemuxType::Enum type,
            BytesStream & stream)
        {
            switch(type)
            {
            case DemuxType::mp4:
                return new Mp4DemuxerBase(stream);
            case DemuxType::asf:
                return new AsfDemuxerBase(stream);
            case DemuxType::flv:
                return new FlvDemuxerBase(stream);
            default:
                assert(0);
                return NULL;
            }
        }

    } // namespace demux
} // namespace ppbox

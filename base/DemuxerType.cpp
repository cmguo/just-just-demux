// PptvDemuxerType.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/source/BytesStream.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/asf/AsfDemuxerBase.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"

namespace ppbox
{
    namespace demux
    {

        DemuxerBase * create_demuxer(
            DemuxerType::Enum type,
            BytesStream & stream)
        {
            switch(type)
            {
            case DemuxerType::mp4:
                return new Mp4DemuxerBase(stream);
            case DemuxerType::asf:
                return new AsfDemuxerBase(stream);
            case DemuxerType::flv:
                return new FlvDemuxerBase(stream);
            default:
                assert(0);
                return NULL;
            }
        }

    } // namespace demux
} // namespace ppbox

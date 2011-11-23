// PptvDemuxerType.cpp

#include "ppbox/demux/pptv/PptvDemuxerType.h"

#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/live/LiveDemuxer.h"
#include "ppbox/demux/live2/Live2Demuxer.h"

namespace ppbox
{
    namespace demux
    {

        PptvDemuxer * pptv_create_demuxer(
            std::string const & proto)
        {
            static std::map<std::string, PptvDemuxerType::Enum> type_map;
            if (type_map.empty()) {
                type_map_["ppvod"] = PptvDemuxerType::vod;
                type_map_["pplive"] = PptvDemuxerType::live;
                type_map_["pplive2"] = PptvDemuxerType::live2;
                type_map_["pptest"] = PptvDemuxerType::test;
            }
            std::map<std::string, PptvDemuxerType::Enum>::const_iterator iter = 
                type_map_.find(proto);
            if (iter != type_map_.end()) {
                demux_type = iter->second;
            }
            PptvDemuxer * demuxer = NULL;
            switch (demux_type) {
                case PptvDemuxerType::vod:
#ifdef PPBOX_DISABLE_VOD
                    demuxer = new VodDemuxer(io_svc(), 0, buffer_size_, prepare_size_);
#else
                    demuxer = new VodDemuxer(io_svc(), vod_.port(), buffer_size_, prepare_size_);
#endif
                    break;
#ifndef PPBOX_DISABLE_LIVE
                case PptvDemuxerType::live:
                    demuxer = new LiveDemuxer(io_svc(), live_.port(), buffer_size_, prepare_size_);
                    break;
#endif
                case PptvDemuxerType::live2:
                    demuxer = new Live2Demuxer(io_svc(), 0, buffer_size_, prepare_size_);
                    break;
                default:
                    demuxer = new EmptyDemuxer(io_svc());
                    assert(0);
            }
            return demuxer;
        }
    
    } // namespace demux
} // namespace ppbox

// PptvDemuxerType.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/pptv/PptvDemuxerType.h"

#include "ppbox/demux/EmptyDemuxer.h"
#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/live/LiveDemuxer.h"
#include "ppbox/demux/live2/Live2Demuxer.h"

namespace ppbox
{
    namespace demux
    {

        PptvDemuxer * pptv_create_demuxer(
            std::string const & proto,
            boost::asio::io_service & io_svc,
            boost::uint16_t port,
            boost::uint32_t buffer_size,
            boost::uint32_t prepare_size)
        {
            static std::map<std::string, PptvDemuxerType::Enum> type_map;
            if (type_map.empty()) {
                type_map["ppvod"] = PptvDemuxerType::vod;
                type_map["pplive"] = PptvDemuxerType::live;
                type_map["pplive2"] = PptvDemuxerType::live2;
                type_map["pptest"] = PptvDemuxerType::test;
            }
            PptvDemuxerType::Enum demux_type = PptvDemuxerType::none;
            std::map<std::string, PptvDemuxerType::Enum>::const_iterator iter = 
                type_map.find(proto);
            if (iter != type_map.end()) {
                demux_type = iter->second;
            }
            PptvDemuxer * demuxer = NULL;
            switch (demux_type) {
                case PptvDemuxerType::vod:
#ifdef PPBOX_DISABLE_VOD
                    demuxer = new VodDemuxer(io_svc, 0, buffer_size, prepare_size);
#else
                    demuxer = new VodDemuxer(io_svc, port, buffer_size, prepare_size);
#endif
                    break;
#ifndef PPBOX_DISABLE_LIVE
                case PptvDemuxerType::live:
                    demuxer = new LiveDemuxer(io_svc, port, buffer_size, prepare_size);
                    break;
#endif
                case PptvDemuxerType::live2:
                    demuxer = new Live2Demuxer(io_svc, 0, buffer_size, prepare_size);
                    break;
                default:
                    demuxer = new EmptyDemuxer(io_svc);
                    assert(0);
            }
            return demuxer;
        }
    
    } // namespace demux
} // namespace ppbox

// CommonDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/CommonDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        CommonDemuxer * CommonDemuxer::create(
            util::daemon::Daemon & daemon,
            std::string const & proto,
            boost::uint32_t buffer_size,
            boost::uint32_t prepare_size)
        {
            static std::map<std::string, SourceType::Enum> type_map;
            if (type_map.empty()) {
                type_map["http"] = PptvDemuxerType::vod;
                type_map["file"] = PptvDemuxerType::live;
                type_map["pipe"] = PptvDemuxerType::live2;
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
                    demuxer = new VodDemuxer(daemon.io_svc(), 0, buffer_size, prepare_size);
#else
                    demuxer = new VodDemuxer(daemon.io_svc(), util::daemon::use_module<ppbox::vod::Vod>(daemon).port(), buffer_size, prepare_size);
#endif
                    break;
#ifndef PPBOX_DISABLE_LIVE
                case PptvDemuxerType::live:
                    demuxer = new LiveDemuxer(daemon.io_svc(), util::daemon::use_module<ppbox::live::Live>(daemon).port(), buffer_size, prepare_size);
                    break;
#endif
                case PptvDemuxerType::live2:
                    demuxer = new Live2Demuxer(daemon.io_svc(), 0, buffer_size, prepare_size);
                    break;
                default:
                    demuxer = new EmptyDemuxer(daemon.io_svc());
                    assert(0);
            }
        virtual boost::system::error_code open(
            std::string const & name, 
            boost::system::error_code & ec)
        {
            std::vector<std::string> key_playlink;
            slice<std::string>(name, std::inserter(
                key_playlink, key_playlink.end()), "|");
            assert(key_playlink.size() > 0);
            std::string playlink = key_playlink[key_playlink.size()-1];
            source_->set_name(playlink);
            return BufferDemuxer::open(ec);
        }

        virtual void async_open(
            std::string const & name, 
            BufferDemuxer::open_response_type const & resp)
        {
            std::vector<std::string> key_playlink;
            slice<std::string>(name, std::inserter(
                key_playlink, key_playlink.end()), "|");
            assert(key_playlink.size() > 0);
            std::string playlink = key_playlink[key_playlink.size()-1];
            source_->set_name(playlink);
            BufferDemuxer::async_open(resp);
        }

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_COMMON_DEMUXER_H_

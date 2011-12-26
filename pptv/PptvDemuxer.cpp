// PptvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/pptv/PptvDemuxer.h"
#include "ppbox/demux/base/BufferList.h"
#include "ppbox/demux/base/SourceBase.h"

#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/live/LiveDemuxer.h"
#include "ppbox/demux/live2/Live2Demuxer.h"

#include "ppbox/vod/Vod.h"
#include "ppbox/live/Live.h"

#include <framework/logger/LoggerSection.h>
using namespace framework::logger;

#include <boost/system/error_code.hpp>
using namespace boost::system;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        PptvDemuxer * PptvDemuxer::create(
            util::daemon::Daemon & daemon,
            framework::string::Url const & url,
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
                type_map.find(url.protocol());
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
                    assert(0);
            }
            std::string params;
            url.param(params);
            demuxer->set_param(params);
            return demuxer;
        }
    
        PptvDemuxer::PptvDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            SourceBase * segmentbase)
            : BufferDemuxer(io_svc, buffer_size, prepare_size, segmentbase)
            , io_svc_(io_svc)
        {
        }

        PptvDemuxer::~PptvDemuxer()
        {
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec)
                : ec_(ec)
                , returned_(false)
            {
            }

            void operator()(
                error_code const & ec)
            {
                boost::mutex::scoped_lock lock(mutex_);
                ec_ = ec;
                returned_ = true;
                cond_.notify_all();
            }

            void wait()
            {
                boost::mutex::scoped_lock lock(mutex_);
                while (!returned_)
                    cond_.wait(lock);
            }

            error_code & ec_;
            bool returned_;
            boost::mutex mutex_;
            boost::condition_variable cond_;
        };

        error_code PptvDemuxer::open(
            std::string const & name, 
            error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(name, boost::ref(resp));
            resp.wait();

            return ec;
        }

        boost::system::error_code PptvDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        boost::system::error_code PptvDemuxer::set_max_dl_speed(
            boost::uint32_t speed, // KBps
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

    }
}

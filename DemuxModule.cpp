// DemuxerModule.cpp

#include "just/demux/Common.h"
#include "just/demux/DemuxModule.h"
#include "just/demux/Version.h"
#include "just/demux/basic/DemuxerTypes.h"
//#include "just/demux/EmptyDemuxer.h"
#include "just/demux/base/DemuxerBase.h"
#include "just/demux/single/SingleDemuxer.h"
#include "just/demux/segment/SegmentDemuxer.h"
#include "just/demux/packet/PacketDemuxer.h"
#ifndef JUST_DISABLE_FFMPEG
#  include "just/demux/ffmpeg/FFMpegDemuxer.h"
#endif

using namespace just::avformat::error;

#include <just/data/base/MediaBase.h>

#include <just/common/UrlHelper.h>

#include <framework/process/Environments.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.DemuxModule", framework::logger::Debug);

namespace just
{
    namespace demux
    {

        struct DemuxModule::DemuxInfo
        {
            just::data::MediaBase * media;
            DemuxerBase * demuxer;
            framework::string::Url play_link;
            error_code ec;

            DemuxInfo()
                : media(NULL)
                , demuxer(NULL)
            {
            }

            struct Finder
            {
                Finder(
                    DemuxerBase * demuxer)
                    : demuxer_(demuxer)
                {
                }

                bool operator()(
                    DemuxInfo const * info)
                {
                    return info->demuxer == demuxer_;
                }

            private:
                DemuxerBase * demuxer_;
            };
       };

        DemuxModule::DemuxModule(
            util::daemon::Daemon & daemon)
            : just::common::CommonModuleBase<DemuxModule>(daemon, "DemuxModule")
        {
            buffer_size_ = 20 * 1024 * 1024;
        }

        DemuxModule::~DemuxModule()
        {
            boost::mutex::scoped_lock lock(mutex_);
            for (size_t i = demuxers_.size() - 1; i != (size_t)-1; --i) {
                priv_destroy(demuxers_[i]);
            }
        }

        error_code DemuxModule::startup()
        {
            error_code ec;
            return ec;
        }

        void DemuxModule::shutdown()
        {
            boost::mutex::scoped_lock lock(mutex_);
            error_code ec;
            for (size_t i = demuxers_.size() - 1; i != (size_t)-1; --i) {
                demuxers_[i]->demuxer->cancel(ec);
            }
        }

        DemuxerBase * DemuxModule::create(
            framework::string::Url const & play_link, 
            framework::string::Url const & config, 
            error_code & ec)
        {
            DemuxInfo * info = priv_create(play_link, config, ec);
            return info ? info->demuxer : NULL;
        }

        bool DemuxModule::destroy(
            DemuxerBase * demuxer, 
            error_code & ec)
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = 
                std::find_if(demuxers_.begin(), demuxers_.end(), DemuxInfo::Finder(demuxer));
            //assert(iter != demuxers_.end());
            if (iter == demuxers_.end()) {
                ec = framework::system::logic_error::item_not_exist;
            } else {
                priv_destroy(*iter);
                ec.clear();
            }
            return !ec;
        }

        DemuxerBase * DemuxModule::find(
            framework::string::Url const & play_link)
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                if ((*iter)->play_link == play_link) {
                    return (*iter)->demuxer;
                }
            }
            return NULL;
        }

        DemuxerBase * DemuxModule::find(
            just::data::MediaBase const & media)
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                if ((*iter)->media == &media) {
                    return (*iter)->demuxer;
                }
            }
            return NULL;
        }

        DemuxModule::DemuxInfo * DemuxModule::priv_create(
            framework::string::Url const & play_link, 
            framework::string::Url const & config, 
            error_code & ec)
        {
            framework::string::Url playlink(play_link);
            if (!just::common::decode_url(playlink, ec))
                return NULL;
            just::data::MediaBase * media = just::data::MediaBase::create(io_svc(), playlink, ec);
            DemuxerBase * demuxer = NULL;
            if (media != NULL) {
                just::data::MediaBasicInfo info;
                if (media->get_basic_info(info, ec)) {
                    if ((info.flags & info.f_extend) == info.f_segment) {
                        demuxer = new SegmentDemuxer(io_svc(), *(just::data::SegmentMedia *)media);
                    } else if ((info.flags & info.f_extend) == info.f_packet) {
                        demuxer = PacketDemuxerFactory::create(info.format_type, io_svc(), *(just::data::PacketMedia *)media, ec);
                    } else {
#ifndef JUST_DISABLE_FFMPEG
                        if (framework::process::get_environment("JUST_DISABLE_FFMPEG", "no") != "yes")
                            demuxer = new FFMpegDemuxer(io_svc(), *media);
#endif
                        if (demuxer == NULL)
                            demuxer = new SingleDemuxer(io_svc(), *media);
                    }
                    if (demuxer) {
                        just::common::apply_config(demuxer->get_config(), config, "demux.");
                    }
                }
            }
            if (demuxer) {
                DemuxInfo * info = new DemuxInfo;
                info->media = media;
                info->demuxer = demuxer;
                info->play_link = playlink;
                boost::mutex::scoped_lock lock(mutex_);
                demuxers_.push_back(info);
                return info;
            }
            return NULL;
        }

        void DemuxModule::priv_destroy(
            DemuxInfo * info)
        {
            DemuxerBase * demuxer = info->demuxer;
            if (demuxer)
                delete demuxer;
            if (info->media)
                delete info->media;
            demuxers_.erase(
                std::remove(demuxers_.begin(), demuxers_.end(), info), 
                demuxers_.end());
            delete info;
            info = NULL;
        }

        void DemuxModule::set_download_buffer_size(
            boost::uint32_t buffer_size)
        {
            buffer_size_ = buffer_size;
        }

    } // namespace demux
} // namespace just

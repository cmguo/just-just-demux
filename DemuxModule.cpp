// DemuxerModule.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxModule.h"
#include "ppbox/demux/Version.h"
#include "ppbox/demux/basic/DemuxerTypes.h"
//#include "ppbox/demux/EmptyDemuxer.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/single/SingleDemuxer.h"
#include "ppbox/demux/segment/SegmentDemuxer.h"
#include "ppbox/demux/packet/PacketDemuxer.h"
//#include "ppbox/demux/ffmpeg/FFMpegDemuxer.h"

using namespace ppbox::avformat::error;

#include <ppbox/data/base/MediaBase.h>

#include <ppbox/common/UrlHelper.h>

#include <framework/timer/Timer.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.DemuxModule", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        struct DemuxModule::DemuxInfo
        {
            ppbox::data::MediaBase * media;
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
            : ppbox::common::CommonModuleBase<DemuxModule>(daemon, "DemuxModule")
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
            ppbox::data::MediaBase const & media)
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
            ppbox::common::decode_url(playlink, ec);
            ppbox::data::MediaBase * media = ppbox::data::MediaBase::create(io_svc(), playlink, ec);
            DemuxerBase * demuxer = NULL;
            if (media != NULL) {
                ppbox::data::MediaBasicInfo info;
                if (media->get_basic_info(info, ec)) {
                    if ((info.flags & info.f_extend) == info.f_segment) {
                        demuxer = new SegmentDemuxer(io_svc(), *(ppbox::data::SegmentMedia *)media);
                    } else if ((info.flags & info.f_extend) == info.f_packet) {
                        demuxer = PacketDemuxerFactory::create(info.format, io_svc(), *(ppbox::data::PacketMedia *)media, ec);
                    } else {
                        demuxer = new SingleDemuxer(io_svc(), *media);
                        //demuxer = new FFMpegDemuxer(io_svc(), *media);
                    }
                    if (demuxer) {
                        ppbox::common::apply_config(demuxer->get_config(), config, "demux.");
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
} // namespace ppbox

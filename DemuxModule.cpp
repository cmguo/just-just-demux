// DemuxerModule.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxModule.h"
#include "ppbox/demux/Version.h"
//#include "ppbox/demux/CommonDemuxer.h"
#include "ppbox/demux/base/DemuxerTypes.h"
#include "ppbox/demux/base/SegmentDemuxer.h"
using namespace ppbox::demux;

#include "ppbox/data/MediaBase.h"
#include "ppbox/data/SourceBase.h"

#include <framework/timer/Timer.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.DemuxerModule", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        struct DemuxModule::DemuxInfo
        {
            enum StatusEnum
            {
                closed, 
                opening, 
                canceled, 
                opened, 
            };

            size_t id;
            StatusEnum status;
            SegmentDemuxer * demuxer;
            framework::string::Url play_link;
            DemuxModule::open_response_type resp;
            error_code ec;

            DemuxInfo(
                SegmentDemuxer * demuxer)
                : status(closed)
                , demuxer(demuxer)
            {
                static size_t sid = 0;
                id = ++sid;
            }

            struct Finder
            {
                Finder(
                    size_t id)
                    : id_(id)
                {
                }

                bool operator()(
                    DemuxInfo const * info)
                {
                    return info->id == id_;
                }

            private:
                size_t id_;
            };
        };

        DemuxModule::DemuxModule(
            util::daemon::Daemon & daemon)
            : ppbox::common::CommonModuleBase<DemuxModule>(daemon, "DemuxerModule")
        {
            buffer_size_ = 20 * 1024 * 1024;
        }

        DemuxModule::~DemuxModule()
        {
        }

        error_code DemuxModule::startup()
        {
            error_code ec;
            return ec;
        }

        void DemuxModule::shutdown()
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (size_t i = demuxers_.size() - 1; i != (size_t)-1; --i) {
                error_code ec;
                close_locked(demuxers_[i], false, ec);
            }
            /*while (!demuxers_.empty()) {
                cond_.wait(lock);
            }*/
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec, 
                SegmentDemuxer *& demuxer, 
                boost::condition_variable & cond, 
                boost::mutex & mutex)
                : ec_(ec)
                , demuxer_(demuxer)
                , cond_(cond)
                , mutex_(mutex)
                , is_return_(false)
            {
            }

            void operator()(
                error_code const & ec, 
                SegmentDemuxer * demuxer)
            {
                boost::mutex::scoped_lock lock(mutex_);
                ec_ = ec;
                demuxer_ = demuxer;
                is_return_ = true;

                cond_.notify_all();
            }

            void wait(
                boost::mutex::scoped_lock & lock)
            {
                while (!is_return_)
                    cond_.wait(lock);
            }

        private:
            error_code & ec_;
            SegmentDemuxer *& demuxer_;
            boost::condition_variable & cond_;

            boost::mutex & mutex_;
            bool is_return_;
        };

        SegmentDemuxer * DemuxModule::open(
            framework::string::Url const & play_link, 
            size_t & close_token, 
            error_code & ec)
        {
            SegmentDemuxer * demuxer = NULL;
            SyncResponse resp(ec, demuxer, cond_, mutex_);
            DemuxInfo * info = create(play_link, boost::ref(resp), ec);
            close_token = info->id;
            boost::mutex::scoped_lock lock(mutex_);
            demuxers_.push_back(info);
            if (!ec) {
                async_open(lock, info);
                resp.wait(lock);
            }
            return demuxer;
        }

        void DemuxModule::async_open(
            framework::string::Url const & play_link, 
            size_t & close_token, 
            open_response_type const & resp)
        {
            error_code ec;
            DemuxInfo * info = create(play_link, resp, ec);
            close_token = info->id;
            boost::mutex::scoped_lock lock(mutex_);
            demuxers_.push_back(info);
            if (ec) {
                io_svc().post(boost::bind(resp, ec, info->demuxer));
            } else {
                async_open(lock, info);
            }
        }

        error_code DemuxModule::close(
            size_t id, 
            error_code & ec)
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = 
                std::find_if(demuxers_.begin(), demuxers_.end(), DemuxInfo::Finder(id));
            //assert(iter != demuxers_.end());
            if (iter == demuxers_.end()) {
                ec = framework::system::logic_error::item_not_exist;
            } else {
                close_locked(*iter, false, ec);
            }
            return ec;
        }

        DemuxModule::DemuxInfo * DemuxModule::create(
            framework::string::Url const & play_link, 
            open_response_type const & resp, 
            error_code & ec)
        {
            ppbox::data::MediaBase * media = ppbox::data::MediaBase::create(io_svc(), play_link);
            SegmentDemuxer * demuxer = new SegmentDemuxer(io_svc(), *media);
            boost::mutex::scoped_lock lock(mutex_);
            DemuxInfo * info = new DemuxInfo(demuxer);
            info->play_link = play_link;
            info->resp = resp;
            return info;
        }

        void DemuxModule::async_open(
            boost::mutex::scoped_lock & lock, 
            DemuxInfo * info)
        {
            SegmentDemuxer * demuxer = info->demuxer;
            lock.unlock();
            demuxer->async_open(
                boost::bind(&DemuxModule::handle_open, this, _1, info));
            lock.lock();
            info->status = DemuxInfo::opening;
        }

        void DemuxModule::handle_open(
            error_code const & ecc,
            DemuxInfo * info)
        {
            boost::mutex::scoped_lock lock(mutex_);

            error_code ec = ecc;
            
            SegmentDemuxer * demuxer = info->demuxer;

            open_response_type resp;
            resp.swap(info->resp);
            //要放在close_locked之前对info->ec赋值
            info->ec = ec;
            if (info->status == DemuxInfo::canceled) {
                close_locked(info, true, ec);
                ec = boost::asio::error::operation_aborted;
            } else {
                info->status = DemuxInfo::opened;
            }

            lock.unlock();

            resp(ec, demuxer);
        }

        error_code DemuxModule::close_locked(
            DemuxInfo * info, 
            bool inner_call, 
            error_code & ec)
        {
            assert(!inner_call || info->status == DemuxInfo::opening || info->status == DemuxInfo::canceled);
            if (info->status == DemuxInfo::closed) {
                ec = error::not_open;
            } else if (info->status == DemuxInfo::opening) {
                info->status = DemuxInfo::canceled;
                cancel(info, ec);
            } else if (info->status == DemuxInfo::canceled) {
                if (inner_call) {
                    info->status = DemuxInfo::closed;
                    ec.clear();
                } else {
                    ec = error::not_open;
                }
            } else if (info->status == DemuxInfo::opened) {
                info->status = DemuxInfo::closed;
            }
            if (info->status == DemuxInfo::closed) {
                close(info, ec);
                destory(info);
            }
            return ec;
        }

        error_code DemuxModule::cancel(
            DemuxInfo * info, 
            error_code & ec)
        {
            SegmentDemuxer * demuxer = info->demuxer;
            demuxer->cancel(ec);
            cond_.notify_all();
            return ec;
        }

        error_code DemuxModule::close(
            DemuxInfo * info, 
            error_code & ec)
        {
            SegmentDemuxer * demuxer = info->demuxer;
            demuxer->close(ec);
            return ec;
        }

        void DemuxModule::destory(
            DemuxInfo * info)
        {
            SegmentDemuxer * demuxer = info->demuxer;
            delete demuxer;
            demuxer = NULL;
            demuxers_.erase(
                std::remove(demuxers_.begin(), demuxers_.end(), info), 
                demuxers_.end());
            delete info;
            info = NULL;
            cond_.notify_all();
        }

        SegmentDemuxer * DemuxModule::find(
            framework::string::Url const & play_link)
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = demuxers_.begin();
            for (size_t i = demuxers_.size() - 1; i != (size_t)-1; --i) {
                if ((*iter)->play_link == play_link) {
                    return (*iter)->demuxer;
                }
            }
            return NULL;
        }

        void DemuxModule::set_download_buffer_size(
            boost::uint32_t buffer_size)
        {
            buffer_size_ = buffer_size;
        }

    } // namespace demux
} // namespace ppbox

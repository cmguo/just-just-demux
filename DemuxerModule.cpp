// DemuxerModule.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxerModule.h"
#include "ppbox/demux/Version.h"
//#include "ppbox/demux/CommonDemuxer.h"
//#include "ppbox/demux/pptv/PptvDemuxer.h"
#include "ppbox/demux/base/BufferDemuxer.h"
using namespace ppbox::demux;
#ifndef PPBOX_DISABLE_DAC
#include <ppbox/dac/Dac.h>
#endif

#include <framework/timer/Timer.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/network/Resolver.h>
using namespace framework::logger;
using namespace framework::network;

#include <util/archive/TextIArchive.h>
#include <util/archive/TextOArchive.h>

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("DemuxerModule", 0);

#ifndef PPBOX_DNS_VOD_JUMP
#define PPBOX_DNS_VOD_JUMP "(tcp)(v4)jump.150hi.com:80"
#endif

namespace ppbox
{
    namespace demux
    {

        struct DemuxerModule::DemuxInfo
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
            BufferDemuxer * demuxer;
            std::string play_link;
            DemuxerModule::open_response_type resp;
            error_code ec;

            DemuxInfo(
                BufferDemuxer * demuxer)
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

        DemuxerModule::DemuxerModule(
            util::daemon::Daemon & daemon)
            : ppbox::common::CommonModuleBase<DemuxerModule>(daemon, "DemuxerModule")
        {
            buffer_size_ = 20 * 1024 * 1024;
            prepare_size_ = 10 * 1024;
            buffer_time_ = 3000; // 3s
            max_dl_speed_ = boost::uint32_t(-1);
        }

        DemuxerModule::~DemuxerModule()
        {
        }

        error_code DemuxerModule::startup()
        {
            error_code ec;
            return ec;
        }

        void DemuxerModule::shutdown()
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
                BufferDemuxer *& demuxer, 
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
                BufferDemuxer * demuxer)
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
            BufferDemuxer *& demuxer_;
            boost::condition_variable & cond_;

            boost::mutex & mutex_;
            bool is_return_;
        };

        BufferDemuxer * DemuxerModule::open(
            std::string const & play_link, 
            size_t & close_token, 
            error_code & ec)
        {
            BufferDemuxer * demuxer = NULL;
            SyncResponse resp(ec, demuxer, cond_, mutex_);
            DemuxInfo * info = create(play_link, boost::ref(resp), ec);
            close_token = info->id;
            if (!ec) {
                boost::mutex::scoped_lock lock(mutex_);
                async_open(info);
                resp.wait(lock);
            }
            return demuxer;
        }

        void DemuxerModule::async_open(
            std::string const & play_link, 
            size_t & close_token, 
            open_response_type const & resp)
        {
            error_code ec;
            DemuxInfo * info = create(play_link, resp, ec);
            close_token = info->id;
            if (ec) {
                io_svc().post(boost::bind(resp, ec, info->demuxer));
            } else {
                boost::mutex::scoped_lock lock(mutex_);
                async_open(info);
            }
        }

        error_code DemuxerModule::close(
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

        DemuxerModule::DemuxInfo * DemuxerModule::create(
            std::string const & play_link, 
            open_response_type const & resp, 
            error_code & ec)
        {
            framework::string::Url url(play_link);
            Content * root_source = Content::create(get_daemon().io_svc(), url);
            BufferDemuxer * demuxer = create(buffer_size_, prepare_size_, root_source);
            boost::mutex::scoped_lock lock(mutex_);
            // new shared_stat需要加锁
            DemuxInfo * info = new DemuxInfo(demuxer);
            info->play_link = play_link;
            info->status = DemuxInfo::opening;
            info->resp = resp;
            demuxers_.push_back(info);
            return info;
        }

        BufferDemuxer * DemuxerModule::create(
            boost::uint32_t buffer_size,
            boost::uint32_t prepare_size,
            Content * source)
        {
            return new BufferDemuxer(get_daemon().io_svc(), buffer_size, prepare_size, source);
        }

        void DemuxerModule::async_open(
            DemuxInfo * info)
        {
            error_code ec;
            BufferDemuxer * demuxer = info->demuxer;
            demuxer->set_time_out(5 * 1000, ec); // 5 seconds
            if (!ec) {
                demuxer->async_open(
                    boost::bind(&DemuxerModule::handle_open, this, _1, info));
            } else {
                io_svc().post(boost::bind(&DemuxerModule::handle_open, this, ec, info));
            }
        }

        void DemuxerModule::handle_open(
            error_code const & ecc,
            DemuxInfo * info)
        {
            boost::mutex::scoped_lock lock(mutex_);

            error_code ec = ecc;
            
            BufferDemuxer * demuxer = info->demuxer;

            ec || demuxer->set_non_block(true, ec);

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

        error_code DemuxerModule::close_locked(
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

        error_code DemuxerModule::cancel(
            DemuxInfo * info, 
            error_code & ec)
        {
            BufferDemuxer * demuxer = info->demuxer;
            demuxer->cancel(ec);
            cond_.notify_all();
            return ec;
        }

        error_code DemuxerModule::close(
            DemuxInfo * info, 
            error_code & ec)
        {
            BufferDemuxer * demuxer = info->demuxer;
            if (info->play_link.empty()) //表示demuxer曾经做过open,所以需要close
                demuxer->close(ec);
            return ec;
        }

        void DemuxerModule::destory(
            DemuxInfo * info)
        {
            BufferDemuxer * demuxer = info->demuxer;
            delete demuxer;
            demuxer = NULL;
            demuxers_.erase(
                std::remove(demuxers_.begin(), demuxers_.end(), info), 
                demuxers_.end());
            delete info;
            info = NULL;
            cond_.notify_all();
        }

        void DemuxerModule::set_download_buffer_size(
            boost::uint32_t buffer_size)
        {
            buffer_size_ = buffer_size;
        }

        void DemuxerModule::set_download_max_speed(
            boost::uint32_t speed)
        {
            max_dl_speed_ = speed;
        }

        void DemuxerModule::set_http_proxy(
            char const * addr)
        {
            http_proxy_.from_string(addr);
        }

        void DemuxerModule::set_play_buffer_time(
            boost::uint32_t buffer_time)
        {
            buffer_time_ = buffer_time;
        }

    } // namespace demux
} // namespace ppbox

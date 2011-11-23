// DemuxerModule.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxerModule.h"
#include "ppbox/demux/pptv/PptvDemuxer.h"
using namespace ppbox::demux;

#include <ppbox/dac/Dac.h>

#include <framework/timer/Timer.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;
using namespace framework::network;

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
            PptvDemuxer * demuxer;
            std::string play_link;
            bool dac_sent_;
            DemuxerModule::open_response_type resp;
            error_code ec;

            DemuxInfo(
                PptvDemuxer * demuxer)
                : status(closed)
                , demuxer(demuxer)
                , dac_sent_(false)
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

        static void handle_resolver(
            error_code const & ec, 
            framework::network::ResolverIterator const & iterator)
        {
        }

        DemuxerModule::DemuxerModule(
            util::daemon::Daemon & daemon)
#ifdef PPBOX_DISABLE_CERTIFY           
            : ppbox::common::CommonModuleBase<DemuxerModule>(daemon, "DemuxerModule")
#else  
            : ppbox::certify::CertifyUserModuleBase<DemuxerModule>(daemon, "DemuxerModule")
#endif
            , dac_(util::daemon::use_module<ppbox::dac::Dac>(daemon))
            , timer_(NULL)
        {
            buffer_size_ = 20 * 1024 * 1024;
            prepare_size_ = 10 * 1024;
            buffer_time_ = 3000; // 3s
            max_dl_speed_ = boost::uint32_t(-1);

            const NetName dns_vod_jump_server(PPBOX_DNS_VOD_JUMP);

            framework::network::ResolverService & service = 
                boost::asio::use_service<framework::network::ResolverService>(io_svc());
            std::vector<framework::network::Endpoint> endpoints;
            endpoints.push_back(framework::network::Endpoint("61.158.254.137", 80));
            endpoints.push_back(framework::network::Endpoint("118.123.212.30", 80));
            service.insert_name(dns_vod_jump_server, endpoints);

            framework::network::Resolver resolver(io_svc());
            resolver.async_resolve(dns_vod_jump_server, handle_resolver);
        }

        DemuxerModule::~DemuxerModule()
        {
        }

        error_code DemuxerModule::startup()
        {
            timer_ = new framework::timer::PeriodicTimer(
                timer_queue(), 1000, boost::bind(&DemuxerModule::handle_timer, this));
            timer_->start();
#ifdef PPBOX_DISABLE_CERTIFY
			return error_code();
#else
            return start_certify();
#endif
        }

        void DemuxerModule::shutdown()
        {
            timer_->stop();
            delete timer_;
            timer_ = NULL;
#ifndef PPBOX_DISABLE_CERTIFY			
            stop_certify();
#endif
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (size_t i = demuxers_.size() - 1; i != (size_t)-1; --i) {
                error_code ec;
                close_locked(demuxers_[i], false, ec);
            }
            while (!demuxers_.empty()) {
                cond_.wait(lock);
            }
        }

        void DemuxerModule::certify_startup()
        {
 #ifdef PPBOX_DISABLE_CERTIFY
 			return;
 #else
            boost::mutex::scoped_lock lock(mutex_);
            cond_.notify_all();
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                //DemuxInfo * info = *iter;
                //PptvDemuxer * demuxer = info->demuxer;
                std::string key;
                error_code ec;
                //if (cert_.certify_url(info->cert_type, info->play_link, key, ec)) {
                //    demuxer->on_extern_error(ec);
                //}
            }
#endif			
        }

        void DemuxerModule::certify_shutdown(
            error_code const & ec)
        {
#ifdef PPBOX_DISABLE_CERTIFY
 			return;
#else   
            boost::mutex::scoped_lock lock(mutex_);
            cond_.notify_all();
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                PptvDemuxer & demuxer = *(*iter)->demuxer;
                demuxer.on_extern_error(ec);
            }
#endif			
        }

        void DemuxerModule::certify_failed(
            error_code const & ec)
        {
            DemuxerModule::certify_shutdown(ec);
        }

        void DemuxerModule::handle_timer()
        {
            boost::mutex::scoped_lock lock(mutex_);
            std::vector<DemuxInfo *>::const_iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                DemuxInfo * info = *iter;
                PptvDemuxer * demuxer = info->demuxer;
                if (info->status == DemuxInfo::opened && !info->dac_sent_ && demuxer->is_ready()) {
                    dac_.play_open_info(info->ec, demuxer);
                    info->dac_sent_ = true;
                }
            }
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec, 
                PptvDemuxer *& demuxer, 
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
                PptvDemuxer * demuxer)
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
            PptvDemuxer *& demuxer_;
            boost::condition_variable & cond_;

            boost::mutex & mutex_;
            bool is_return_;
        };

        PptvDemuxer * DemuxerModule::open(
            std::string const & play_link, 
            size_t & close_token, 
            error_code & ec)
        {
            PptvDemuxer * demuxer = NULL;
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
            std::string::size_type pos_colon = play_link.find("://");
            std::string proto = "ppvod";
            if (pos_colon == std::string::npos) {
                pos_colon = 0;
            } else {
                proto = play_link.substr(0, pos_colon);
                pos_colon += 3;
            }
            std::string::size_type pos_param = play_link.find('?');
            if (pos_param == std::string::npos) {
                pos_param = play_link.length() - pos_colon;
            } else {
                pos_param -= pos_colon;
            }

            PptvDemuxer * demuxer = pptv_create_demuxer(proto);
            if (demuxer == NULL) {
                //demuxer = new EmptyDemuxer();
            }
            demuxer->set_param(play_link.substr(pos_param+1));
            boost::mutex::scoped_lock lock(mutex_);
            // new shared_stat需要加锁
            DemuxInfo * info = new DemuxInfo(demuxer);
            info->play_link = play_link.substr(pos_colon, pos_param);
            info->status = DemuxInfo::opening;
            info->resp = resp;
            demuxers_.push_back(info);
            return info;
        }

        void DemuxerModule::async_open(
            DemuxInfo * info)
        {
            error_code ec;
#ifndef PPBOX_DISABLE_CERTIFY
            if (is_alive()) {
                LOG_S(Logger::kLevelEvent, "ppbox_alive: success");
            } else {
                LOG_S(Logger::kLevelAlarm, "ppbox_alive: failure");
            }
#endif
            //while (info->status == DemuxInfo::opening && !is_certified(ec)) {
            //    cond_.wait(lock);
            //}
#ifndef PPBOX_DISABLE_CERTIFY
            is_certified(ec);
#endif			

            if (info->status == DemuxInfo::canceled) {
                ec = boost::asio::error::operation_aborted;
            }
            PptvDemuxer * demuxer = info->demuxer;
            std::string key;
#ifdef PPBOX_DISABLE_CERTIFY
            key = info->cert_type == ppbox::certify::CertifyType::vod ? "kioe257ds":"pplive";
#endif			
            ec
#ifndef PPBOX_DISABLE_CERTIFY			   	
                //|| cert_.certify_url(info->cert_type, info->play_link, key, ec)
#endif
                //|| demuxer->set_time_out(5 * 1000, ec) // 5 seconds
                || (!http_proxy_.host().empty() && demuxer->set_http_proxy(http_proxy_, ec));
            if (!ec) {
                //demuxer->set_non_block(true, ec);

                std::string play_link;
                play_link.swap(info->play_link);
                demuxer->async_open(
                    key + "|" + play_link, 
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

            PptvDemuxer * demuxer = info->demuxer;

            open_response_type resp;
            resp.swap(info->resp);
            //要放在close_locked之前对info->ec赋值
            info->ec = ec;
            if (info->status == DemuxInfo::canceled) {
                close_locked(info, true, ec);
                ec = boost::asio::error::operation_aborted;
            } else {
                info->status = DemuxInfo::opened;
                if (!info->dac_sent_ && demuxer->is_ready()) {
                    dac_.play_open_info(info->ec, demuxer);
                    info->dac_sent_ = true;
                }
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
            PptvDemuxer * demuxer = info->demuxer;
            demuxer->cancel(ec);
            cond_.notify_all();
            return ec;
        }

        error_code DemuxerModule::close(
            DemuxInfo * info, 
            error_code & ec)
        {
            PptvDemuxer * demuxer = info->demuxer;
            if (info->play_link.empty()) //表示demuxer曾经做过open,所以需要close
                demuxer->close(ec);
            if (!info->dac_sent_) {
                dac_.play_open_info(info->ec, demuxer);
                info->dac_sent_ = true;
            }
            dac_.play_close_info(demuxer);
            return ec;
        }

        void DemuxerModule::destory(
            DemuxInfo * info)
        {
            PptvDemuxer * demuxer = info->demuxer;
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

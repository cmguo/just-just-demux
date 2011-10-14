// DemuxerModule.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxerModule.h"
#include "ppbox/demux/DemuxerError.h"
#include "ppbox/demux/DemuxerStatistic.h"
#include "ppbox/demux/VodDemuxer.h"
#include "ppbox/demux/LiveDemuxer.h"
#include "ppbox/demux/FileDemuxer.h"
#include "ppbox/demux/EmptyDemuxer.h"
using namespace ppbox::demux;

#include <ppbox/dac/Dac.h>
#include <ppbox/vod/Vod.h>
#include <ppbox/live/Live.h>

#include <framework/timer/Timer.h>
#include <framework/memory/MemoryReference.h>
#include <framework/memory/SharedMemoryIdPointer.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;
using namespace framework::network;

#include <boost/bind.hpp>
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("DemuxerModule", 0);

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
            Demuxer * demuxer;
            SharedStatistics * shared_stat;
            certify::CertifyType::Enum cert_type;
            std::string play_link;
            bool dac_sent_;
            DemuxerModule::open_response_type resp;
            error_code ec;

            DemuxInfo(
                Demuxer * demuxer, 
                certify::CertifyType::Enum cert_type)
                : status(closed)
                , demuxer(demuxer)
                , shared_stat(new SharedStatistics)
                , cert_type(cert_type)
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
            : ppbox::certify::CertifyUserModuleBase<DemuxerModule>(daemon, "DemuxerModule")
            , dac_(util::daemon::use_module<ppbox::dac::Dac>(daemon))
            , live_(util::daemon::use_module<ppbox::live::Live>(daemon))
            , vod_(util::daemon::use_module<ppbox::vod::Vod>(daemon))
            , stats_(NULL)
            , timer_(NULL)
        {
            buffer_size_ = 20 * 1024 * 1024;
            prepare_size_ = 10 * 1024;
            buffer_time_ = 3000; // 3s
            max_dl_speed_ = boost::uint32_t(-1);

            type_map_["ppvod"] = DemuxerType::vod;
            type_map_["pplive"] = DemuxerType::live;
            type_map_["ppfile-mp4"] = DemuxerType::mp4;
            type_map_["ppfile-asf"] = DemuxerType::asf;

            SharedStatistics::set_pool(framework::memory::BigFixedPool(
                framework::memory::MemoryReference<framework::memory::SharedMemory>(shared_memory())));
            stats_ = (framework::container::List<SharedStatistics> *)shared_memory()
                .alloc_with_id(SHARED_OBJECT_ID_DEMUX, sizeof(framework::container::List<SharedStatistics>));
            if (!stats_)
                stats_ = (framework::container::List<SharedStatistics> *)shared_memory()
                .get_by_id(SHARED_OBJECT_ID_DEMUX);
            new (stats_) framework::container::List<SharedStatistics>;

#ifdef API_PPLIVE
            const NetName dns_vod_jump_server("(tcp)(v4)dt.api.pplive.com:80");
#else
            const NetName dns_vod_jump_server("(tcp)(v4)jump.150hi.com:80");
#endif

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
            SharedStatistics::set_pool(
                framework::memory::BigFixedPool(framework::memory::PrivateMemory()));
        }

        error_code DemuxerModule::startup()
        {
            timer_ = new framework::timer::PeriodicTimer(
                timer_queue(), 1000, boost::bind(&DemuxerModule::handle_timer, this));
            timer_->start();
            return start_certify();
        }

        void DemuxerModule::shutdown()
        {
            timer_->stop();
            delete timer_;
            timer_ = NULL;
            stop_certify();
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
            boost::mutex::scoped_lock lock(mutex_);
            cond_.notify_all();
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                DemuxInfo * info = *iter;
                Demuxer * demuxer = info->demuxer;
                std::string key;
                error_code ec;
                if (cert_.certify_url(info->cert_type, info->play_link, key, ec)) {
                    demuxer->on_extern_error(ec);
                }
            }
        }

        void DemuxerModule::certify_shutdown(
            error_code const & ec)
        {
            boost::mutex::scoped_lock lock(mutex_);
            cond_.notify_all();
            std::vector<DemuxInfo *>::iterator iter = demuxers_.begin();
            for (; iter != demuxers_.end(); ++iter) {
                Demuxer & demuxer = *(*iter)->demuxer;
                demuxer.on_extern_error(ec);
            }
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
                Demuxer * demuxer = info->demuxer;
                if (info->status == DemuxInfo::opened && !info->dac_sent_ && demuxer->is_ready()) {
                    dac_.play_open_info(info->ec, demuxer);
                    info->dac_sent_ = true;
                }
                info->shared_stat->buf_stat = demuxer->buffer_stat();
                info->shared_stat->demux_stat = demuxer->stat();
            }
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec, 
                Demuxer *& demuxer, 
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
                Demuxer * demuxer)
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
            Demuxer *& demuxer_;
            boost::condition_variable & cond_;

            boost::mutex & mutex_;
            bool is_return_;
        };

        Demuxer * DemuxerModule::open(
            std::string const & play_link, 
            size_t & close_token, 
            error_code & ec)
        {
            Demuxer * demuxer = NULL;
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

            DemuxerType::Enum demux_type = DemuxerType::none;
            certify::CertifyType::Enum cert_type = certify::CertifyType::local;
            {
                std::map<std::string, DemuxerType::Enum>::const_iterator iter = 
                    type_map_.find(proto);
                if (iter != type_map_.end()) {
                    demux_type = iter->second;
                }
            }

            Demuxer * demuxer = NULL;
            switch (demux_type) {
                case DemuxerType::vod:
                    cert_type = certify::CertifyType::vod;
                    demuxer = new VodDemuxer(io_svc(), vod_.port(), buffer_size_, prepare_size_);
                    break;
                case DemuxerType::live:
                    cert_type = certify::CertifyType::live;
                    demuxer = new LiveDemuxer(io_svc(), live_.port(), buffer_size_, prepare_size_);
                    break;
                case DemuxerType::mp4:
                    cert_type = certify::CertifyType::local;
                    demuxer = new Mp4FileDemuxer(io_svc(), buffer_size_, prepare_size_);
                    break;
                case DemuxerType::asf:
                    cert_type = certify::CertifyType::local;
                    demuxer = new AsfFileDemuxer(io_svc(), buffer_size_, prepare_size_);
                    break;
                default:
                    cert_type = certify::CertifyType::local;
                    demuxer = new EmptyDemuxer(io_svc());
                    assert(0);
            }
            if (demuxer) {
                boost::mutex::scoped_lock lock(mutex_);

                std::string::size_type pos_param = play_link.find('?');
                if (pos_param == std::string::npos) {
                    pos_param = play_link.length() - pos_colon;
                } else {
                    demuxer->set_param(play_link.substr(pos_param+1));
                    pos_param -= pos_colon;
                }

                // new shared_stat需要加锁
                DemuxInfo * info = new DemuxInfo(demuxer, cert_type);
                info->play_link = play_link.substr(pos_colon, pos_param);
                info->status = DemuxInfo::opening;
                info->resp = resp;
                demuxers_.push_back(info);
                stats_->insert(info->shared_stat);
                return info;
            }
            return NULL;
        }

        void DemuxerModule::async_open(
            DemuxInfo * info)
        {
            error_code ec;

            if (is_alive()) {
                LOG_S(Logger::kLevelEvent, "ppbox_alive: success");
            } else {
                LOG_S(Logger::kLevelAlarm, "ppbox_alive: failure");
            }

            if (vod_.is_alive()) {
                LOG_S(Logger::kLevelEvent, "vod_worker: success");
            } else {
                LOG_S(Logger::kLevelAlarm, "vod_worker: failure");
            }

            if (live_.is_alive()) {
                LOG_S(Logger::kLevelEvent, "live_worker: success");
            } else {
                LOG_S(Logger::kLevelAlarm, "live_worker: failure");
            }

            //while (info->status == DemuxInfo::opening && !is_certified(ec)) {
            //    cond_.wait(lock);
            //}
            is_certified(ec);

            if (info->status == DemuxInfo::canceled) {
                ec = boost::asio::error::operation_aborted;
            }
            Demuxer * demuxer = info->demuxer;
            std::string key;
            ec || cert_.certify_url(info->cert_type, info->play_link, key, ec)
                || demuxer->set_time_out(5 * 1000, ec) // 5 seconds
                || (!http_proxy_.host().empty() && demuxer->set_http_proxy(http_proxy_, ec));
            if (!ec) {
                demuxer->set_non_block(true, ec);

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

            Demuxer * demuxer = info->demuxer;

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
            Demuxer * demuxer = info->demuxer;
            demuxer->cancel(ec);
            cond_.notify_all();
            return ec;
        }

        error_code DemuxerModule::close(
            DemuxInfo * info, 
            error_code & ec)
        {
            Demuxer * demuxer = info->demuxer;
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
            Demuxer * demuxer = info->demuxer;
            delete demuxer;
            demuxer = NULL;
            stats_->erase(info->shared_stat);
            delete info->shared_stat;
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

// DemuxerModule.h

#ifndef _PPBOX_DEMUX_DEMUXER_MODULE_H_
#define _PPBOX_DEMUX_DEMUXER_MODULE_H_

#include "ppbox/demux/pptv/PptvDemuxerType.h"

#include <ppbox/certify/CertifyUserModule.h>
#include <ppbox/certify/Certifier.h>

#include <framework/network/NetName.h>
#include <framework/container/List.h>
#include <framework/memory/SharedMemoryPointer.h>
#include <framework/memory/MemoryPoolObject.h>
#include <framework/memory/BigFixedPool.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/function.hpp>

#define SHARED_OBJECT_ID_DEMUX 2

namespace framework
{
    namespace timer { class Timer; }
}

namespace ppbox
{
#ifndef PPBOX_DISABLE_DAC
    namespace dac { class Dac; }
#endif
    namespace demux
    {

        class BufferDemuxer;

        class DemuxerModule
#ifdef PPBOX_DISABLE_CERTIFY
            : public ppbox::common::CommonModuleBase<DemuxerModule>
#else
            : public ppbox::certify::CertifyUserModuleBase<DemuxerModule>
#endif            
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &, 
                BufferDemuxer *)
            > open_response_type;

        public:
            DemuxerModule(
                util::daemon::Daemon & daemon);

            ~DemuxerModule();

        public:
            virtual boost::system::error_code startup();

            virtual void shutdown();

        public:
            // 进入认证成功状态
            virtual void certify_startup();

            // 退出认证成功状态
            virtual void certify_shutdown(
                boost::system::error_code const & ec);

            // 认证出错
            virtual void certify_failed(
                boost::system::error_code const & ec);

        public:
            void set_download_buffer_size(
                boost::uint32_t buffer_size);

            void set_download_max_speed(
                boost::uint32_t speed);

            void set_http_proxy(
                char const * addr);

            void set_play_buffer_time(
                boost::uint32_t buffer_time);

        public:
            BufferDemuxer * open(
                std::string const & play_link, 
                size_t & close_token, 
                boost::system::error_code & ec);

            void async_open(
                std::string const & play_link, 
                size_t & close_token, 
                open_response_type const & resp);

            boost::system::error_code close(
                size_t close_token, 
                boost::system::error_code & ec);

            static BufferDemuxer * find(std::string name);

        public:
            struct DemuxInfo;

        private:
            DemuxInfo * create(
                std::string const & play_link, 
                open_response_type const & resp, 
                boost::system::error_code & ec);

            void async_open(
                DemuxInfo * info);

            void handle_open(
                boost::system::error_code const & ec,
                DemuxInfo * info);

            boost::system::error_code close_locked(
                DemuxInfo * info, 
                bool inner_call, 
                boost::system::error_code & ec);

            boost::system::error_code close(
                DemuxInfo * info, 
                boost::system::error_code & ec);

            boost::system::error_code cancel(
                DemuxInfo * info, 
                boost::system::error_code & ec);

            void destory(
                DemuxInfo * info);

            void handle_timer();

        private:
            // 配置
            boost::uint32_t buffer_size_;
            boost::uint32_t prepare_size_;
            boost::uint32_t buffer_time_;
            boost::uint32_t max_dl_speed_;
            framework::network::NetName http_proxy_;

        private:
#ifndef PPBOX_DISABLE_DAC
            dac::Dac & dac_;
#endif

#ifndef PPBOX_DISABLE_PEER
            peer::Peer & peer_;
#endif

        private:
            framework::timer::Timer * timer_;
            std::vector<DemuxInfo *> demuxers_;
            boost::mutex mutex_;
            boost::condition_variable cond_;

            // specify record demuxer
            static BufferDemuxer * record_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_MODULE_H_

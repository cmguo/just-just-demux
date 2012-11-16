// DemuxerModule.h

#ifndef _PPBOX_DEMUX_DEMUXER_MODULE_H_
#define _PPBOX_DEMUX_DEMUXER_MODULE_H_

#include <framework/string/Url.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/function.hpp>

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;
        class Strategy;

        class DemuxModule
            : public ppbox::common::CommonModuleBase<DemuxModule>
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &, 
                DemuxerBase *)
            > open_response_type;

        public:
            DemuxModule(
                util::daemon::Daemon & daemon);

            ~DemuxModule();

        public:
            virtual boost::system::error_code startup();

            virtual void shutdown();

        public:
            void set_download_buffer_size(
                boost::uint32_t buffer_size);

        public:
            DemuxerBase * open(
                framework::string::Url const & play_link, 
                size_t & close_token, 
                boost::system::error_code & ec);

            void async_open(
                framework::string::Url const & play_link, 
                size_t & close_token, 
                open_response_type const & resp);

            boost::system::error_code close(
                size_t close_token, 
                boost::system::error_code & ec);

            DemuxerBase * find(
                framework::string::Url const & play_link);

        private:
            struct DemuxInfo;

        private:
            DemuxInfo * create(
                framework::string::Url const & play_link, 
                open_response_type const & resp, 
                boost::system::error_code & ec);

            void async_open(
                boost::mutex::scoped_lock & lock, 
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

        private:
            // ≈‰÷√
            boost::uint32_t buffer_size_;

        private:
            std::vector<DemuxInfo *> demuxers_;
            boost::mutex mutex_;
            boost::condition_variable cond_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_MODULE_H_

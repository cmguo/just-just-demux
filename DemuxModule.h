// DemuxerModule.h

#ifndef _PPBOX_DEMUX_DEMUXER_MODULE_H_
#define _PPBOX_DEMUX_DEMUXER_MODULE_H_

#include <framework/string/Url.h>

#include <boost/thread/mutex.hpp>
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
            DemuxerBase * create(
                framework::string::Url const & play_link, 
                framework::string::Url const & config, 
                boost::system::error_code & ec);

            bool destroy(
                DemuxerBase * demuxer, 
                boost::system::error_code & ec);

            DemuxerBase * find(
                framework::string::Url const & play_link);

        private:
            struct DemuxInfo;

        private:
            DemuxInfo * priv_create(
                framework::string::Url const & play_link, 
                framework::string::Url const & config, 
                boost::system::error_code & ec);

            void priv_destroy(
                DemuxInfo * info);

        private:
            // ≈‰÷√
            boost::uint32_t buffer_size_;

        private:
            std::vector<DemuxInfo *> demuxers_;
            boost::mutex mutex_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_MODULE_H_

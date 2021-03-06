// DemuxModule.h

#ifndef _JUST_DEMUX_DEMUX_MODULE_H_
#define _JUST_DEMUX_DEMUX_MODULE_H_

#include <framework/string/Url.h>

#include <boost/thread/mutex.hpp>

namespace just
{
    namespace data
    {
        class MediaBase;
    }

    namespace demux
    {

        class DemuxerBase;
        class Strategy;

        class DemuxModule
            : public just::common::CommonModuleBase<DemuxModule>
        {
        public:
            DemuxModule(
                util::daemon::Daemon & daemon);

            ~DemuxModule();

        public:
            virtual bool startup(
                boost::system::error_code & ec);

            virtual bool shutdown(
                boost::system::error_code & ec);

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

            DemuxerBase * find(
                just::data::MediaBase const & media);

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
            // ����
            boost::uint32_t buffer_size_;

        private:
            std::vector<DemuxInfo *> demuxers_;
            boost::mutex mutex_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_DEMUX_MODULE_H_

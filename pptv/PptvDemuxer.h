// PptvDemuxer.h

#ifndef _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_
#define _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_

#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/pptv/PptvDemuxerStatistic.h"


namespace framework { namespace timer { class Ticker; } }
namespace framework { namespace network { class NetName; } }

namespace ppbox
{
    namespace demux
    {

        class PptvDemuxer
            : public BufferDemuxer
            , public PptvDemuxerStatistic
        {
        public:
            static PptvDemuxer * create(
                util::daemon::Daemon & daemon,
                framework::string::Url const & url,
                boost::uint32_t buffer_size,
                boost::uint32_t prepare_size);
        public:
            PptvDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size,
                SourceBase * segmentbase);

            virtual ~PptvDemuxer();

        public:
            virtual boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec);

            virtual void async_open(
                std::string const & name, 
                open_response_type const & resp) = 0;

            virtual boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

            virtual boost::system::error_code set_max_dl_speed(
                boost::uint32_t speed, // KBps
                boost::system::error_code & ec);

        public:
            BufferStatistic const & buffer_stat() const
            {
                return (BufferStatistic&)(*BufferDemuxer::buffer_);
            }

        private:
            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;
            boost::system::error_code extern_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_

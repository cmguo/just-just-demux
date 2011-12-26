// CommonDemuxer.h

#ifndef _PPBOX_DEMUX_COMMON_DEMUXER_H_
#define _PPBOX_DEMUX_COMMON_DEMUXER_H_

#include "ppbox/demux/base/BufferDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        class CommonDemuxer
            : public BufferDemuxer
        {
        public:
            static create(
                util::daemon::Daemon & daemon,
                std::string const & proto,
                boost::uint32_t buffer_size,
                boost::uint32_t prepare_size);

        public:
            CommonDemuxer(
                boost::asio::io_service & io_svc, 
                SourceBase * source)
                : BufferDemuxer(io_svc, source)
                , source_(source)
            {
            }

            virtual ~CommonDemuxer()
            {
                delete source_;
            }

        public:
            virtual boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec);

            virtual void async_open(
                std::string const & name, 
                BufferDemuxer::open_response_type const & resp);

        private:
            Source * source_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_COMMON_DEMUXER_H_

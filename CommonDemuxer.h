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
            static BufferDemuxer *  create(
                util::daemon::Daemon & daemon,                std::string const & url_str,
                boost::uint32_t buffer_size,
                boost::uint32_t prepare_size);

        public:
            CommonDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size, 
                SourceBase * source, 
                std::string const & url_str )
                //: BufferDemuxer(io_svc, buffer_size, prepare_size, source)
                : BufferDemuxer(io_svc, buffer_size, prepare_size)
                , source_(source)
                , url_str_(url_str)
            {
                source_->set_buffer_list(buffer_);
            }

            virtual ~CommonDemuxer()
            {
                delete source_;
            }

        public:
            //virtual boost::system::error_code open(
            //    std::string const & name, 
            //    boost::system::error_code & ec);

            //virtual void async_open(
            //    std::string const & name, 
            //    BufferDemuxer::open_response_type const & resp);

        private:
            SourceBase * source_;
            std::string url_str_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_COMMON_DEMUXER_H_

// EmptyDemuxer.h

#ifndef _PPBOX_DEMUX_EMPTY_DEMUXER_H_
#define _PPBOX_DEMUX_EMPTY_DEMUXER_H_

#include "ppbox/demux/base/BufferDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        class EmptySource()
            : public SourceBase
        {
        private:
            virtual void next_segment(
                SegmentPositionEx &)
            {
            }
        }

        class EmptyDemuxer
            : public BufferDemuxer
        {
        public:
            EmptyDemuxer(
                boost::asio::io_service & io_svc)
                : BufferDemuxer(io_svc, &source_)
            {
            }

            ~EmptyDemuxer()
            {
            }

        public:
            boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec)
            {
                ec = ppbox::demux::error::bad_file_type;
                return ec;
            }

            void async_open(
                std::string const & name, 
                open_response_type const & resp)
            {
                boost::system::error_code ec;
                open(name, ec);
                io_svc_.post(boost::bind(resp, ec));
            }

            boost::system::error_code close(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

        private:
            EmptySource source_;
        };
    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_EMPTY_DEMUXER_H_


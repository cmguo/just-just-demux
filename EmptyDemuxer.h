// EmptyDemuxer.h

#ifndef _PPBOX_DEMUX_EMPTY_DEMUXER_H_
#define _PPBOX_DEMUX_EMPTY_DEMUXER_H_

#include "ppbox/demux/base/SegmentDemuxer.h"

#include <boost/bind.hpp>
#include <boost/utility/base_from_member.hpp>

namespace ppbox
{
    namespace demux
    {

        class EmptyMedia
            : public ppbox::data::MediaBase
        {
        public:
            EmptyMedia(
                boost::asio::io_service & io_svc)
                : MediaBase(io_svc)
                , proto_("http")
            {
            }

            std::string const & get_protocol() const
            {
                return proto_;
            }

            virtual void async_open(
                response_type const & resp)
            {
            }

            virtual void cancel(
                boost::system::error_code & ec)
            {
            }

            virtual void close(
                boost::system::error_code & ec)
            {
            }

            virtual bool get_info(
                ppbox::data::MediaInfo & info,
                boost::system::error_code & ec) const
            {
                return false;
            }

            virtual size_t segment_count() const
            {
                return 0;
            }

            virtual bool segment_url(
                size_t segment, 
                framework::string::Url & url,
                boost::system::error_code & ec) const
            {
                return false;
            }

            virtual void segment_info(
                size_t segment, 
                ppbox::data::SegmentInfo & info) const
            {
            }

        private:
            std::string proto_;
        };

        class EmptyDemuxer
            : boost::base_from_member<EmptyMedia>
            , public SegmentDemuxer
        {
        public:
            EmptyDemuxer(
                boost::asio::io_service & io_svc)
                : boost::base_from_member<EmptyMedia>(boost::ref(io_svc))
                , SegmentDemuxer(io_svc, member)
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
        };
    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_EMPTY_DEMUXER_H_


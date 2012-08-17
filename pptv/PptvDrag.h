// PptvDrag.h

#ifndef _PPBOX_DEMUX_PPTV_PPTV_DRAG_H_
#define _PPBOX_DEMUX_PPTV_PPTV_DRAG_H_

#include <ppbox/cdn/HttpStatistics.h>

#include <util/protocol/http/HttpClient.h>

#include <framework/network/NetName.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace ppbox
{
    namespace demux
    {

        class PptvDrag
            : public boost::enable_shared_from_this<PptvDrag>
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &, 
                boost::asio::streambuf &)
            > response_type;

        public:
            PptvDrag(
                boost::asio::io_service & io_svc);

            ~PptvDrag();

        public:
            void async_get(
                framework::string::Url const & url, 
                framework::network::NetName const & server_host, 
                response_type const & resp);

            ppbox::cdn::HttpStatistics const & http_stat() const
            {
                return http_stat_;
            }

            boost::asio::streambuf & get_buf()
            {
                return http_.response().data();
            }

            void cancel();

        private:
            void handle_fetch(
                boost::system::error_code const & ec);

            void response(
                boost::system::error_code const & ec);

        private:
            util::protocol::HttpClient http_;
            framework::network::NetName server_host_;
            size_t returned_;
            ppbox::cdn::HttpStatistics http_stat_;
            bool canceled_;
            response_type resp_;

            size_t try_times_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PPTV_PPTV_DRAG_H_

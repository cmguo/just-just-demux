// VodJump.h

#ifndef _PPBOX_DEMUX_PPTV_PPTV_JUMP_H_
#define _PPBOX_DEMUX_PPTV_PPTV_JUMP_H_

#include <ppbox/common/HttpStatistics.h>

#include <util/protocol/http/HttpClient.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>

namespace ppbox
{
    namespace demux
    {

        struct JumpType
        {
            enum Enum
            {
                live = 1, 
                vod,
            };
        };

        class PptvJump
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > response_type;

        public:
            PptvJump(
                boost::asio::io_service & io_svc,
                JumpType::Enum type);

            ~PptvJump();

        public:
            void async_get(
                framework::string::Url const & url, 
                response_type const & resp);

            ppbox::common::HttpStatistics const & http_stat() const
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
            size_t returned_;
            ppbox::common::HttpStatistics http_stat_;

            JumpType::Enum jump_type_;

            bool canceled_;

            response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PPTV_PPTV_JUMP_H_

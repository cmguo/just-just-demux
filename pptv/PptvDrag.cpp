// VodDrag.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/pptv/PptvDrag.h"
using namespace ppbox::demux::error;

#include <util/protocol/http/HttpRequest.h>
#include <util/protocol/http/HttpHead.h>
using namespace util::protocol;

#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::network;
using namespace framework::logger;

#include <boost/asio/buffer.hpp>
#include <boost/bind.hpp>
using namespace boost::system;
using namespace boost::asio;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvDrag", 0);

namespace ppbox
{
    namespace demux
    {
        PptvDrag::PptvDrag(
            boost::asio::io_service & io_svc)
            : http_(io_svc)
            , returned_(0)
            , canceled_(false)
            , try_times_(0)
        {
        }

        PptvDrag::~PptvDrag()
        {
        }

        void PptvDrag::async_get(
            framework::string::Url const & url, 
            framework::network::NetName const & server_host, 
            response_type const & resp)
        {
            server_host_ = server_host;
            resp_ = resp;
            //HTTP
            HttpRequestHead request_head;

            request_head.method = HttpRequestHead::get;
            request_head.path = url.path_all();
            request_head.host.reset(url.host_svc());
            //request_head.host.reset(server_host_.host_svc());
            request_head["Accept"] = "{*/*}";

            std::ostringstream oss;
            request_head.get_content(oss);
            LOG_STR(framework::logger::Logger::kLevelDebug1, oss.str().c_str());

            http_stat_.begin_try();
            http_.async_fetch(request_head,
                boost::bind(&PptvDrag::handle_fetch, shared_from_this(), _1));
        }

        void PptvDrag::handle_fetch(
            error_code const & ec)
        {
            http_stat_.end_try(http_.stat(), ec);
            if (!ec) {
            } else {
                if (!canceled_ && (++try_times_ == 1 || util::protocol::HttpClient::recoverable(ec))) {
                    LOG_S(Logger::kLevelDebug, "[handle_fetch] ec: " << ec.message());
                    http_stat_.begin_try();
                    http_.request_head().host.reset(server_host_.host_svc());
                    http_.async_fetch(http_.request_head(),
                        boost::bind(&PptvDrag::handle_fetch, shared_from_this(), _1));
                    return;
                }
                LOG_S(Logger::kLevelAlarm, "[handle_fetch] ec: " << ec.message());
            }
            returned_ = 1;
            response(ec);
        }

        void PptvDrag::cancel()
        {
            error_code ec1;
            http_.cancel(ec1);

            canceled_ = true;
        }

        void PptvDrag::response(
            error_code const & ec)
        {
            response_type resp;
            resp.swap(resp_);
            resp(ec, http_.response().data());
        }

    } // namespace demux
} // namespace ppbox

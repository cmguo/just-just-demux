// VodJump.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/PptvJump.h"
using namespace ppbox::demux::error;

#include <framework/string/Format.h>
#include <framework/network/NetName.h>
#include <framework/system/BytesOrder.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::string;
using namespace framework::network;
using namespace framework::logger;

#include <util/protocol/http/HttpRequest.h>
using namespace util::protocol;

#include <boost/asio/buffer.hpp>
#include <boost/bind.hpp>
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvJump", 0);

namespace ppbox
{
    namespace demux
    {

        PptvJump::PptvJump(
            boost::asio::io_service & io_svc,
            JumpType::Enum type)
            : http_(io_svc)
            , returned_(0)
            , jump_type_(type)
            , canceled_(false)
        {
            srand((boost::uint32_t)time(NULL));
        }

        PptvJump::~PptvJump()
        {
        }

        static unsigned long get_dns_info()
        {
            unsigned long ip = 0;
            char buf[100] = {0};
            FILE * fd = fopen("/etc/resolv.conf", "r");
            if (fd == NULL)
                return 0;

            while (!feof(fd)) {
                if (fgets(buf, 99, fd) == NULL) {
                    break;
                }

                if (strlen(buf) <= 2)
                    continue;

                if (buf[strlen(buf)-2] == '\r'
                    && buf[strlen(buf)-1] == '\n')
                    buf[strlen(buf)-2] = '\0';

                if (buf[strlen(buf)-1] == '\r'
                    || buf[strlen(buf)-1] == '\n')
                    buf[strlen(buf)-1] = '\0';

                std::vector<std::string> cmd_args;
                slice<std::string>(buf, std::inserter(cmd_args, cmd_args.end()), " ");

                std::vector<std::string>::const_iterator iter = 
                    std::remove(cmd_args.begin(), cmd_args.end(), std::string());

                if (iter - cmd_args.begin() == 2 
                    && cmd_args[0] == "nameserver") {
                    ip = framework::network::NetName::ip_pton(cmd_args[1]);

                    unsigned char * ip_addr = (unsigned char *)&ip;
                    if (ip_addr[0] == 10
                        || (ip_addr[0] == 172 && (ip_addr[1] >= 16 && ip_addr[1] <= 31))
                        || (ip_addr[0] == 192 && ip_addr[1] == 168)
                        || (ip_addr[0] == 127 && ip_addr[1] == 0 && ip_addr[2] == 0 && ip_addr[3] == 1)
                        || (ip_addr[0] == 8 && ip_addr[1] == 8 
                        && ((ip_addr[2] == 8 && ip_addr[3] == 8) || (ip_addr[2] == 4 && ip_addr[3] == 4)))) {
                        ip = 0;
                    } else {
                        ip = framework::system::BytesOrder::little_endian_to_host(ip);
                    }

                    break;
                }
            }

            fclose(fd);

            return ip;
        }

        void PptvJump::async_get(
            framework::string::Url const & url, 
            response_type const & resp)
        {
            resp_ = resp;

            framework::string::Url url_t = url;

            url_t.param("t", format(rand()));
            url_t.param("type", "ppbox");

            unsigned long dns_ip = get_dns_info();
            if (dns_ip) {
                url_t.param("dns", format(dns_ip));
            }

            util::protocol::HttpRequest request;
            request.head().method = util::protocol::HttpRequestHead::get;
            request.head().path = url_t.path_all();
            request.head().host.reset(url_t.host_svc());
            request.head()["Accept"] = "{*/*}";

            std::ostringstream oss;
            request.head().get_content(oss);
            LOG_STR(framework::logger::Logger::kLevelDebug1, oss.str().c_str());

            http_stat_.begin_try();
            http_.async_fetch(request.head(),
                boost::bind(&PptvJump::handle_fetch, this, _1));
        }

        void PptvJump::handle_fetch(
            error_code const & ec)
        {
            LOG_S(Logger::kLevelDebug, "[handle_fetch] called");

            http_stat_.end_try(http_.stat(), ec);
            if (ec && util::protocol::HttpClient::recoverable(ec) && !canceled_) {
                LOG_S(Logger::kLevelDebug, "[handle_fetch] ec: " << ec.message());
                if (jump_type_ == JumpType::vod) {
                    http_stat_.begin_try();
                    http_.async_refetch(
                        boost::bind(&PptvJump::handle_fetch, this, _1));
                    return;
                }
            } else if (ec) {
                LOG_S(Logger::kLevelAlarm, "[handle_fetch] ec: " << ec.message());
            }
            returned_ = 1;
            response(ec);
        }

        void PptvJump::cancel()
        {
            error_code ec1;
            http_.cancel(ec1);

            canceled_ = true;
        }

        void PptvJump::response(
            error_code const & ec)
        {
            response_type resp;
            resp.swap(resp_);
            resp(ec);
        }

    } // namespace demux
} // namespace ppbox

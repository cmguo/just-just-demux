// PptvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/pptv/PptvDemuxer.h"
#include "ppbox/demux/base/BufferList.h"
#include "ppbox/demux/base/SourceBase.h"

#include <framework/logger/LoggerSection.h>
using namespace framework::logger;

#include <boost/system/error_code.hpp>
using namespace boost::system;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        PptvDemuxer::PptvDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size,
            SourceBase * segmentbase)
            : BufferDemuxer(io_svc, buffer_size, prepare_size, segmentbase)
            , io_svc_(io_svc)
        {
        }

        PptvDemuxer::~PptvDemuxer()
        {
        }

        struct SyncResponse
        {
            SyncResponse(
                error_code & ec)
                : ec_(ec)
                , returned_(false)
            {
            }

            void operator()(
                error_code const & ec)
            {
                boost::mutex::scoped_lock lock(mutex_);
                ec_ = ec;
                returned_ = true;
                cond_.notify_all();
            }

            void wait()
            {
                boost::mutex::scoped_lock lock(mutex_);
                while (!returned_)
                    cond_.wait(lock);
            }

            error_code & ec_;
            bool returned_;
            boost::mutex mutex_;
            boost::condition_variable cond_;
        };

        error_code PptvDemuxer::open(
            std::string const & name, 
            error_code & ec)
        {
            SyncResponse resp(ec);
            async_open(name, boost::ref(resp));
            resp.wait();

            return ec;
        }

        boost::system::error_code PptvDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

        boost::system::error_code PptvDemuxer::set_max_dl_speed(
            boost::uint32_t speed, // KBps
            boost::system::error_code & ec)
        {
            ec = framework::system::logic_error::not_supported;
            return ec;
        }

    }
}

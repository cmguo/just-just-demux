// DemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"

#include <util/daemon/Daemon.h>

namespace ppbox
{
    namespace demux
    {

        DemuxerBase::DemuxerBase(
            boost::asio::io_service & io_svc)
            : config_(util::daemon::Daemon::from_io_svc(io_svc).config(), "ppbox.demux")
            , io_svc_(io_svc)
        {
        }

        DemuxerBase::~DemuxerBase()
        {
        }

    } // namespace demux
} // namespace ppbox

// DemuxerBase.cpp

#include "just/demux/Common.h"
#include "just/demux/base/DemuxerBase.h"

#include <util/daemon/Daemon.h>

namespace just
{
    namespace demux
    {

        DemuxerBase::DemuxerBase(
            boost::asio::io_service & io_svc)
            : config_(util::daemon::Daemon::from_io_svc(io_svc).config(), "just.demux")
            , io_svc_(io_svc)
        {
        }

        DemuxerBase::~DemuxerBase()
        {
        }

    } // namespace demux
} // namespace just

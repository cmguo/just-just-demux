// VodSource.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/vod/VodSource.h"
#include "ppbox/demux/pptv/PptvJump.h"
#include "ppbox/demux/pptv/PptvDrag.h"
#include "ppbox/demux/base/DemuxerError.h"

#include <util/protocol/pptv/Url.h>
#include <util/protocol/pptv/TimeKey.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h>
#include <util/buffers/BufferCopy.h>

#include <framework/string/Parse.h>
#include <framework/string/StringToken.h>
#include <framework/string/Algorithm.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/network/NetName.h>
using namespace framework::system::logic_error;
using namespace framework::string;
using namespace framework::logger;

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/bind.hpp>
#include <boost/thread/condition_variable.hpp>
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("VodSource", 0);

namespace ppbox
{
    namespace demux
    {
        static inline std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        VodSource::VodSource(
            boost::asio::io_service & io_svc,
            ppbox::cdn::SegmentBase * pSegment,
            ppbox::demux::Source * pSource)
            : SourceBase(io_svc, pSegment, pSource)
        {
        }

        VodSource::~VodSource()
        {

        }





        error_code VodSource::time_seek(
            boost::uint64_t time, 
            SegmentPositionEx & abs_position, 
            SegmentPositionEx & position, 
            error_code & ec)
        {
            SourceBase::time_seek(time, abs_position, position, ec);
            if (position.total_state == SegmentPositionEx::not_exist 
                && (get_segment_base()->segment_count()>0)
                && !ec) {
                    ec = framework::system::logic_error::out_of_range;
            }
            return ec;
        }


        DemuxerType::Enum VodSource::demuxer_type() const
        {
            return DemuxerType::mp4;
        }

        error_code VodSource::reset(
            SegmentPositionEx & segment)
        {
            segment.segment = 0;
            segment.source = this;
            segment.shard_beg = segment.size_beg = 0;
            boost::uint64_t seg_size = get_segment_base()->segment_size(segment.segment);
            segment.shard_end = segment.size_end = (seg_size == boost::uint64_t(-1) ? boost::uint64_t(-1): segment.size_beg + seg_size);
            segment.time_beg = segment.time_beg = 0;
            boost::uint64_t seg_time = get_segment_base()->segment_time(segment.segment);
            segment.time_end = (seg_time == boost::uint64_t(-1) ? boost::uint64_t(-1): segment.time_beg + seg_time );
            if (segment.shard_end == boost::uint64_t(-1)) {
                segment.total_state = SegmentPositionEx::not_exist;
            } else {
                segment.total_state = SegmentPositionEx::is_valid;
            }
            if (segment.time_end == boost::uint64_t(-1)) {
                segment.time_state = SegmentPositionEx::not_exist;
            } else {
                segment.time_state = SegmentPositionEx::is_valid;
            }
            begin_segment_ = segment;
            return error_code();
        }


    } // namespace demux
} // namespace ppbox

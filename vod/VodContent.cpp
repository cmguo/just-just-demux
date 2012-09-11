// VodContent.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/vod/VodContent.h"
#include "ppbox/demux/base/DemuxerError.h"

#include <framework/logger/Logger.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("VodContent", 0);

namespace ppbox
{
    namespace demux
    {

        VodContent::VodContent(
            boost::asio::io_service & io_svc,
            ppbox::data::MediaBase * media,
            ppbox::data::SourceBase * source)
            : Content(io_svc, media, source)
        {
        }

        VodContent::~VodContent()
        {

        }

        error_code VodContent::time_seek(
            boost::uint64_t time, 
            SegmentPositionEx & abs_position, 
            SegmentPositionEx & position, 
            error_code & ec)
        {
            Content::time_seek(time, abs_position, position, ec);
            if (position.total_state == SegmentPositionEx::not_exist 
                && (get_media()->segment_count()>0)
                && !ec) {
                    ec = framework::system::logic_error::out_of_range;
            }
            return ec;
        }


        DemuxerType::Enum VodContent::demuxer_type() const
        {
            return DemuxerType::mp4;
        }

        error_code VodContent::reset(
            SegmentPositionEx & segment)
        {
            segment.segment = 0;
            segment.source = this;
            segment.shard_beg = segment.size_beg = 0;

            ppbox::data::SegmentInfo seg_info;
            get_media()->segment_info(segment.segment, seg_info);
            boost::uint64_t seg_size = seg_info.size;
            segment.shard_end = segment.size_end = (seg_size == boost::uint64_t(-1) ? boost::uint64_t(-1): segment.size_beg + seg_size);
            segment.time_beg = segment.time_beg = 0;
            boost::uint64_t seg_time = seg_info.duration;
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

// Live2Source.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/live2/Live2Source.h"
#include "ppbox/demux/pptv/PptvJump.h"
#include "ppbox/demux/base/DemuxerError.h"
#include "ppbox/demux/base/BufferList.h"


#include <util/protocol/pptv/Url.h>
#include <util/protocol/pptv/TimeKey.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h> 

#include <framework/string/Format.h>
#include <framework/timer/Timer.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace boost::system;
using namespace framework::logger;
using namespace ppbox::demux;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live2Source", 0);

#ifndef PPBOX_DNS_LIVE2_JUMP
#  define PPBOX_DNS_LIVE2_JUMP "(tcp)(v4)live.dt.synacast.com:80"
#endif

#define P2P_HEAD_LENGTH 1400

namespace ppbox
{
    namespace demux
    {

        Live2Source::Live2Source(
            boost::asio::io_service & io_svc,
            ppbox::cdn::SegmentBase * pSegment,
            ppbox::demux::Source * pSource)
            : SourceBase(io_svc, pSegment, pSource)
        {
        }

        Live2Source::~Live2Source()
        {
        }


        boost::system::error_code Live2Source::reset(
            SegmentPositionEx & segment)
        {
            segment.source = this;
            get_segment_base()->reset(segment.segment);
            begin_segment_ = segment;
            return error_code();
        }

        DemuxerType::Enum Live2Source::demuxer_type() const
        {
            return DemuxerType::flv;
        }

        boost::system::error_code Live2Source::time_seek (
            boost::uint64_t time, // ОўГо
            SegmentPositionEx & abs_position,
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            ec.clear();
//             boost::uint64_t iTime = 0;
//             position.segment = time / (interval_ * 1000);
//             position.time_beg = time;
//             position.time_end = position.time_beg + (interval_ * 1000);
//             position.time_state = SegmentPositionEx::by_guess;
// 
//             bool find = false;
//             for (boost::uint32_t i = 0; i < segments_.size(); ++i) {
//                 if (segments_[i].segment == position.segment) {
//                     position.source = this;
//                     position.total_state = segments_[i].total_state;
//                     position.size_beg = segments_[i].size_beg;
//                     position.size_end = segments_[i].size_end;
//                     position.time_beg = segments_[i].time_beg;
//                     position.time_end = segments_[i].time_end;
//                     position.shard_beg = segments_[i].shard_beg;
//                     position.shard_end = segments_[i].shard_end;
//                     abs_position = begin_segment_;
//                     find = true;
//                     break;
//                 }
//             }
//             if (!find || position.total_state < SegmentPositionEx::is_valid) {
//                 segments_.clear();
//                 position.source = this;
//                 begin_segment_ = position;
//                 abs_position = position;
//                 get_segment_base()->update_segment(position.segment);
//             }

            return ec;
        }

        bool Live2Source::next_segment(
            SegmentPositionEx & segment)
        {
            bool res = false;
            //res = SourceBase::next_segment(segment);
            segment.segment++;

            boost::uint64_t total_time = source_time_before(segment.segment);
            boost::uint64_t total_size = source_size_before(segment.segment);
            segment.total_state = SegmentPositionEx::is_valid;
            segment.time_state = SegmentPositionEx::is_valid;
            segment.time_beg = total_time;
            segment.time_end = total_time +get_segment_base()->segment_time(segment.segment);
            segment.size_beg = total_size;
            segment.size_end = get_segment_base()->segment_size(segment.segment);
            segment.shard_beg = total_size;
            segment.shard_end = get_segment_base()->segment_size(segment.segment);

            boost::uint32_t out_time = 0;
            res = get_segment_base()->next_segment(segment.segment,out_time);
            if (res && out_time > 0)
            {
                SourceBase::buffer()->pause(out_time*1000);
            }
            return res;
        }

    } // namespace demux
} // namespace ppbox

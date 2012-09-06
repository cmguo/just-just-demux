// Live2Content.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/live2/Live2Content.h"
#include "ppbox/demux/base/DemuxerError.h"
#include "ppbox/demux/base/BufferList.h"


#include <util/protocol/pptv/Url.h>
#include <util/protocol/pptv/TimeKey.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h> 

#include <framework/string/Format.h>
#include <framework/timer/Timer.h>
#include <framework/logger/StreamRecord.h>
using namespace boost::system;
using namespace framework::logger;
using namespace ppbox::demux;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live2Content", 0);

#ifndef PPBOX_DNS_LIVE2_JUMP
#  define PPBOX_DNS_LIVE2_JUMP "(tcp)(v4)live.dt.synacast.com:80"
#endif

#define P2P_HEAD_LENGTH 1400

namespace ppbox
{
    namespace demux
    {

        Live2Content::Live2Content(
            boost::asio::io_service & io_svc,
            ppbox::data::MediaBase * pSegment,
            ppbox::data::SourceBase * pSource)
            : Content(io_svc, pSegment, pSource)
        {
        }

        Live2Content::~Live2Content()
        {
        }


        boost::system::error_code Live2Content::reset(
            SegmentPositionEx & segment)
        {
            segment.source = this;
//             get_segment()->reset(segment.segment);
            begin_segment_ = segment;
            return error_code();
        }

        DemuxerType::Enum Live2Content::demuxer_type() const
        {
            return DemuxerType::flv;
        }

        boost::system::error_code Live2Content::time_seek (
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

//         bool Live2Content::next_segment(
//             SegmentPositionEx & segment)
//         {
//             bool res = false;
//             //res = Content::next_segment(segment);
//             segment.segment++;
// 
//             ppbox::data::SegmentInfo seg_info;
//             get_segment()->segment_info(segment.segment, seg_info);
// 
//             boost::uint64_t total_time = source_time_before(segment.segment);
//             boost::uint64_t total_size = source_size_before(segment.segment);
//             segment.total_state = SegmentPositionEx::is_valid;
//             segment.time_state = SegmentPositionEx::is_valid;
//             segment.time_beg = total_time;
//             segment.time_end = total_time + seg_info.time;
//             segment.size_beg = total_size;
//             segment.size_end = seg_info.size;
//             segment.shard_beg = total_size;
//             segment.shard_end = seg_info.size;
//             boost::uint32_t out_time = 0;
//             res = get_segment()->next_segment(segment.segment,out_time);
//             if (res && out_time > 0)
//             {
//                 Content::buffer()->pause(out_time*1000);
//             }
//             return res;
//         }

    } // namespace demux
} // namespace ppbox

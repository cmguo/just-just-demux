// SourceBase.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/base/BytesStream.h"

#include "ppbox/demux/vod/VodSource.h"
#include "ppbox/demux/live2/Live2Source.h"

#include <framework/system/LogicError.h>

using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("SourceBase", 0);

namespace ppbox
{
    namespace demux
    {

        SourceBase * SourceBase::create(
            boost::asio::io_service & io_svc, 
            std::string const & playlink)
        {
            SourceBase* source = NULL;
            std::string::size_type pos_colon = playlink.find("://");
            std::string proto = "ppvod";
            if (pos_colon == std::string::npos) {
                pos_colon = 0;
            } else {
                proto = playlink.substr(0, pos_colon);
                pos_colon += 3;
            }

            if (proto == "ppvod") {
                source = new VodSource(io_svc);
            } else if (proto == "pplive2") {
                source = new Live2Source(io_svc);
            } else {
                source = new VodSource(io_svc);
            }
            return source;
        }

        void SourceBase::destory(
            SourceBase * sourcebase)
        {
            delete sourcebase;
            sourcebase = NULL;
        }

        SourceBase::SourceBase(
            boost::asio::io_service & io_svc)
            : io_svc_( io_svc )
            , insert_segment_(0)
            , insert_size_(0)
            , insert_delta_(0)
            , insert_time_(0)
            , insert_input_time_(0)
        {
            type_map_["ppvod"] = SourcePrefix::vod;
            type_map_["pplive"] = SourcePrefix::live;
            type_map_["pplive2"] = SourcePrefix::live2;
            type_map_["ppfile-mp4"] = SourcePrefix::file_mp4;
            type_map_["ppfile-asf"] = SourcePrefix::file_asf;
            type_map_["ppfile-flv"] = SourcePrefix::file_flv;
            type_map_["pphttp-mp4"] = SourcePrefix::http_mp4;
            type_map_["pphttp-asf"] = SourcePrefix::http_asf;
            type_map_["pphttp-flv"] = SourcePrefix::http_flv;
            type_map_["ppdesc-mp4"] = SourcePrefix::desc_mp4;
            type_map_["ppdesc-asf"] = SourcePrefix::desc_asf;
            type_map_["ppdesc-flv"] = SourcePrefix::desc_flv;
            type_map_["pprecord"] = SourcePrefix::record;
        }

        SourceBase::~SourceBase()
        {
        }

        void SourceBase::on_event(
            Event const & evt)
        {
            switch ( evt.evt_type )
            {
            case Event::EVENT_SEG_DL_OPEN:
                // 更新插入状态
                // LOG_S(0, "EVENT_SEG_DL_OPEN: seg = " << evt.seg_info.segment << " seg.size_beg = " << evt.seg_info.size_beg << \
                    " seg.size_end = " << evt.seg_info.size_end << " seg.time_beg = " << evt.seg_info.time_beg << " seg.time_end = " <<\
                    evt.seg_info.time_end);
                break;
            case Event::EVENT_SEG_DL_END:
                LOG_S(0, "EVENT_SEG_DL_END: seg = " << evt.seg_info.segment << " seg.size_beg = " << evt.seg_info.size_beg << \
                    " seg.size_end = " << evt.seg_info.size_end << " seg.time_beg = " << evt.seg_info.time_beg << " seg.time_end = " <<\
                    evt.seg_info.time_end);
                update_segment_file_size(
                    evt.seg_info.segment, evt.seg_info.size_end - evt.seg_info.size_beg);
                break;
            case Event::EVENT_SEG_DEMUXER_OPEN:// 分段解封装成功
                // 更新插入状态
                //LOG_S(0, "EVENT_SEG_DEMUXER_OPEN: seg = " << evt.seg_info.segment << " seg.size_beg = " << evt.seg_info.size_beg << \
                    " seg.size_end = " << evt.seg_info.size_end << " seg.time_beg = " << evt.seg_info.time_beg << " seg.time_end = " <<\
                    evt.seg_info.time_end);
                update_segment_duration(
                    evt.seg_info.segment, evt.seg_info.time_end - evt.seg_info.time_beg);
                break;
            case Event::EVENT_SEG_DEMUXER_STOP:
                //LOG_S(0, "EVENT_SEG_DEMUXER_STOP: seg = " << evt.seg_info.segment << " seg.size_beg = " << evt.seg_info.size_beg << \
                    " seg.size_end = " << evt.seg_info.size_end << " seg.time_beg = " << evt.seg_info.time_beg << " seg.time_end = " <<\
                    evt.seg_info.time_end);
                break;
            default:
                break;
            }
        }

        // 判断本分段是否有插入源
        bool SourceBase::has_children(
            SegmentPositionEx const & position)
        {
            if (position.source->first_child()
                && ((SourceBase*)position.source->first_child())->insert_segment() == position.segment) {
                return true;
            }
            return false;
        }

        bool SourceBase::next_segment(
            SegmentPositionEx & position)
        {
            bool res = false;
            if (!position.source) {
                position = begin_segment_;
                position.source = this;
            } else {
                position.segment++;
                boost::uint64_t total_time = source_time_before(position.segment);
                boost::uint64_t total_size = source_size_before(position.segment);
                position.time_beg = total_time;
                position.size_beg = position.shard_beg = total_size;
                if (position.segment < segment_count()) {
                    boost::uint64_t segment_len = segment_size(position.segment);
                    boost::uint64_t segment_duration = segment_time(position.segment);
                    if (segment_len != boost::uint64_t(-1)) {
                        position.size_end = position.shard_end = position.size_beg + segment_len;
                        position.total_state = SegmentPositionEx::is_valid;
                    } else {
                        position.size_end = position.shard_end = boost::uint64_t(-1);
                        position.total_state = SegmentPositionEx::not_exist;
                    }
                    if (segment_duration != boost::uint64_t(-1)) {
                        position.time_end = position.time_beg + segment_time(position.segment);
                        position.time_state = SegmentPositionEx::is_valid;
                    } else {
                        position.time_end = boost::uint64_t(-1);
                        position.time_state = SegmentPositionEx::not_exist;
                    }
                } else {
                    position.size_end = position.shard_end = boost::uint64_t(-1);
                    position.total_state = SegmentPositionEx::not_exist;
                    position.time_end = boost::uint64_t(-1);
                    position.time_state = SegmentPositionEx::not_exist;
                    res = false;
                }
            }
            return true;
        }

        boost::system::error_code SourceBase::time_seek (
            boost::uint64_t time, 
            SegmentPositionEx & abs_position,
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            SegmentPositionEx cur_seg = position, pre_seg = position, abs_pos = abs_position;
            bool ischanged = false;
            SegmentPositionEx first_segment;
            bool res = next_segment(first_segment);
            assert(res); // source 至少有一个segment
            abs_position = first_segment;

            while (next_segment(cur_seg)) {
                if (cur_seg.shard_end == boost::uint64_t(-1) 
                    || cur_seg == abs_pos) {
                    // abs_position = cur_seg;
                    abs_position = pre_seg;
                    ischanged = true;
                } else if (ischanged && (pre_seg.shard_end != boost::uint64_t(-1) || pre_seg == abs_pos) ) {
                    cur_seg.size_beg = cur_seg.shard_beg = 0;
                    cur_seg.size_end = cur_seg.shard_end = segment_size(cur_seg.segment);
                    ischanged = false;
                } else if (pre_seg.shard_beg != 0) {
                    cur_seg.size_beg = cur_seg.shard_beg = pre_seg.shard_end;
                    cur_seg.size_end = cur_seg.shard_end = pre_seg.shard_end + segment_size(cur_seg.segment);
                    ischanged = false;
                }

                if (cur_seg.time_end >= time) {
                    position = cur_seg;
                    break;
                }

                pre_seg = cur_seg;
            }
            return boost::system::error_code();
        }

        boost::system::error_code SourceBase::size_seek (
            boost::uint64_t size, 
            SegmentPositionEx const & abs_position,
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            boost::system::error_code ret_ec = boost::system::error_code();
            SegmentPositionEx cur_seg = abs_position;
            while (next_segment(cur_seg)) {
                if (cur_seg.shard_end >= size) {
                    position = cur_seg;
                    break;
                }
            }
            return ret_ec;
        }

        boost::system::error_code SourceBase::time_insert(
            boost::uint32_t time, 
            SourceBase * source, 
            SegmentPositionEx & position, 
            boost::system::error_code & ec)
        {
            SegmentPositionEx abs_pos;
            time_seek(time, abs_pos, position, ec);
            if (!ec) {
                source->skip_ = true;// 假插入
                source->insert_segment_ = position.segment;// 指向父分段
                source->insert_input_time_ = source->insert_time_ = time - position.time_beg;   // 插入分段上的时间位置（相对）
                position.source->insert_child(source, position.next_child);                     // 插入子节点
            }
            return ec;
        }

        void SourceBase::update_insert(
            SegmentPositionEx const & position, 
            boost::uint32_t time, 
            boost::uint64_t offset, 
            boost::uint64_t delta)
        {
            skip_ = false; // 真插入
            insert_time_ = time - position.time_beg; // 插入的时间点（相对本分段开始位置位置）
            insert_size_ = offset;
            insert_delta_ = delta;
        }

        boost::uint64_t SourceBase::next_end(
            SegmentPositionEx & segment)
        {
            assert(segment.source == this);
            next_skip_source(segment);
            if (segment.source) {
                return segment.source->total_size_before((SourceBase *)segment.next_child);
            } else {
                return (boost::uint64_t)-1;
            }
        }

        boost::uint64_t SourceBase::source_size()
        {
            boost::uint64_t total = 0;
            for (boost::uint32_t i = begin_segment_.segment; i < segment_count(); i++) {
                total += segment_size(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_size_before(
            size_t segment)
        {
            assert(begin_segment_.segment <= segment);
            boost::uint64_t total = 0;
            if (segment > segment_count()) {
                segment = segment_count();
            }
            for (int i = begin_segment_.segment; i < segment; i++) {
                total += segment_size(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_time()
        {
            boost::uint64_t total = 0;
            for (int i = begin_segment_.segment; i < segment_count(); i++) {
                total += segment_time(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::source_time_before(
            size_t segment)
        {
            boost::uint64_t total = 0;
            if (segment > segment_count()) {
                segment = segment_count();
            }
            for (int i = begin_segment_.segment; i < segment; i++) {
                total += segment_time(i);
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_size()
        {
            boost::uint64_t total = source_size();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_size();
                    total += item->insert_delta_;
                }
                item = (SourceBase *)item->next_sibling_;
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_time()
        {
            boost::uint64_t total = source_time();
            SourceBase * item = (SourceBase *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_time();
                    item = (SourceBase *)item->next_sibling_;
                }
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_size_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total += source_size_before(child->insert_segment_) + child->insert_size_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_size();
                        total += item->insert_delta_; // 需要加入前半部分的数据才能正常播放
                    }
                    item = (SourceBase *)item->prev_sibling_;
                }
            } else {
                total += tree_size();
            }
            return total;
        }

        boost::uint64_t SourceBase::tree_time_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total = source_time_before(child->insert_segment_) + child->insert_time_;
                SourceBase * item = (SourceBase *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_time();
                        item = (SourceBase *)item->prev_sibling_;
                    }
                }
            } else {
                total = tree_time();
            }
            return total;
        }

        boost::uint64_t SourceBase::total_size_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((SourceBase *)parent_)->tree_size_before(this);
            }
            total += tree_size_before(child);
            return total;
        }

        boost::uint64_t SourceBase::total_time_before(
            SourceBase * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((SourceBase *)parent_)->total_time_before(this);
            }
            total += tree_time_before(child);
            return total;
        }

    } // namespace demux
} // namespace ppbox

// DemuxStrategy.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/segment/DemuxStrategy.h"
#include "ppbox/demux/base/SourceError.h"

#include <framework/system/LogicError.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
using namespace framework::logger;
using namespace framework::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.DemuxStrategy", Debug);

namespace ppbox
{
    namespace demux
    {

        DemuxStrategy * DemuxStrategy::create(
            boost::asio::io_service & io_svc, 
            framework::string::Url const & playlink)
        {
            return NULL;
        }

        void DemuxStrategy::destory(
            DemuxStrategy * content)
        {
            delete content;
            content = NULL;
        }

        DemuxStrategy::DemuxStrategy(
            ppbox::data::SegmentMedia & media)
            : ppbox::data::SegmentStrategy(media)
            , tree_item_(this)
            , insert_item_(NULL)
            , insert_segment_(0)
            , insert_size_(0)
            , insert_delta_(0)
            , insert_time_(0)
        {
        }

        DemuxStrategy::~DemuxStrategy()
        {
        }

        bool DemuxStrategy::next_segment(
            ppbox::data::SegmentPosition & pos, 
            boost::system::error_code & ec)
        {
            if (!pos.item_context) {
                pos.item_context = &tree_item_;
                pos.index = -1;
                return next_segment(pos, ec);
            }

            SourceTreeItem * tree_item = (SourceTreeItem *)pos.item_context;
            DemuxStrategy * strategy = tree_item->owner();
            if (strategy != this) { // 转发请求
                return strategy->next_segment(pos, ec);
            }
            
            if (tree_item->is_inserted() && tree_item->next_owner()->insert_segment_ == pos.index) { // 父切子
                pos.item_context = tree_item->next();
                strategy = tree_item->next()->owner();
                pos.index = (size_t)-1;
                return strategy->next_segment(pos, ec);
            } else if (++pos.index == media_.segment_count()) { // 子切父
                pos.item_context = tree_item->next();
                if (pos.item_context) {
                    strategy = tree_item->next()->owner();
                    pos.index = insert_segment_ - 1;
                    strategy->next_segment(pos, ec);
                    pos.byte_range.beg = insert_size_ - insert_delta_; // 调整begin，big_offset也要相应调整
                    pos.byte_range.pos = pos.byte_range.beg;
                    pos.byte_range.big_offset -= pos.byte_range.beg;
                    pos.time_range.beg = insert_time_; // 调整time_beg，big_time也要相应调整
                    pos.time_range.pos = pos.time_range.beg;
                    pos.time_range.big_offset -= pos.time_range.beg;
                    return true;
                } else {
                    ec = source_error::no_more_segment;
                    return false; // 没有父节点了
                }
            } else {
                pos.byte_range.before_next();
                pos.time_range.before_next();
                media_.segment_info(pos.index, pos);
                if (pos.url.is_valid())
                    media_.segment_url(pos.index, pos.url, ec);
                pos.byte_range.beg = 0;
                pos.byte_range.pos = 0;
                pos.byte_range.end = pos.size;
                pos.time_range.beg = 0;
                pos.time_range.pos = 0;
                pos.time_range.end = pos.duration;
                pos.byte_range.after_next();
                pos.time_range.after_next();
                if (tree_item->is_inserted() && tree_item->next_owner()->insert_segment_ == pos.index) {
                    pos.byte_range.end = tree_item->next_owner()->insert_size_;
                    pos.time_range.end = tree_item->next_owner()->insert_time_;
                }
                ec.clear();
                return true;
            }
        }

        bool DemuxStrategy::reset(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            ppbox::data::MediaInfo media_info;
            if (!media_.get_info(media_info, ec))
                return false;
            assert(media_info.shift >= media_info.delay);
            time = media_info.type == ppbox::data::MediaInfo::live 
                ? media_info.current - media_info.delay : 0;
            return true;
        }

        bool DemuxStrategy::time_seek (
            boost::uint64_t time, 
            ppbox::data::SegmentPosition & base,
            ppbox::data::SegmentPosition & pos, 
            boost::system::error_code & ec)
        {
            ppbox::data::SegmentPosition old_base = base;

            pos.url.protocol("");

            if (time < pos.time_range.big_beg()) {
                base = ppbox::data::SegmentPosition();
                pos = ppbox::data::SegmentPosition();
                if (!next_segment(pos, ec)) {
                    assert(0);
                    return false;
                }
            }

            while (time >= pos.time_range.big_end()) {
                if (pos.byte_range.end == boost::uint64_t(-1) || pos == old_base) {
                    base = pos;
                    pos.byte_range.big_offset = pos.byte_range.end = 0;
                }
                if (!next_segment(pos, ec)) {
                    return false;
                }
            }

            pos.time_range.pos = time - pos.time_range.big_offset;

            media_.segment_url(pos.index, pos.url, ec);

            return true;
        }
/*
        boost::system::error_code DemuxStrategy::insert(
            SegmentPosition const & pos, 
            DemuxStrategy & child, 
            boost::system::error_code & ec)
        {
            tree_item_.insert_child(&child.tree_item_, NULL);                     // 插入子节点
            child.insert_segment_ = pos.index;
            child.insert_size_ = pos.byte_range.end;
            child.insert_delta_ = pos.byte_range.end - pos.byte_range.beg;
            child.insert_time_ = pos.time_range.beg;   // 插入分段上的时间位置（相对）
            return ec;
        }
*/
/*
        bool DemuxStrategy::size_seek (
            boost::uint64_t size, 
            SegmentPosition const & base,
            SegmentPosition & pos)
        {
            if (size < pos.big_beg()) {
                base = SegmentPosition();
                pos = SegmentPosition();
                if (!next_segment(pos)) {
                    assert(0);
                    return false;
                }
            }

            while (time >= pos.big_end()) {
                if (pos.end == boost::uint64_t(-1) || pos == old_base) {
                    base = pos;
                    pos.big_offset = pos.end = 0;
                }
                if (!next_segment(pos)) {
                    return false;
                }
            }

            return true;
        }

        boost::system::error_code Strategy::time_insert(
            boost::uint32_t time, 
            Strategy * source, 
            SegmentPosition & pos, 
            boost::system::error_code & ec)
        {
            SegmentPosition abs_pos;
            time_seek(time, abs_pos, pos, ec);
            if (!ec) {
                source->skip_ = true;// 假插入
                source->insert_segment_ = pos.pos;// 指向父分段
                source->insert_input_time_ = source->insert_time_ = time - pos.time_beg;   // 插入分段上的时间位置（相对）
                pos.source->insert_child(source, pos.next_child);                     // 插入子节点
            }
            return ec;
        }

        void Strategy::update_insert(
            SegmentPosition const & pos, 
            boost::uint32_t time, 
            boost::uint64_t offset, 
            boost::uint64_t delta)
        {
            skip_ = false; // 真插入
            insert_time_ = time - pos.time_beg; // 插入的时间点（相对本分段开始位置位置）
            insert_size_ = offset;
            insert_delta_ = delta;
        }

        boost::uint64_t Strategy::next_end(
            SegmentPosition & pos)
        {
            assert(pos.source == this);
            next_skip_source(pos);
            if (pos.source) {
                return pos.source->total_size_before((Strategy *)pos.next_child);
            } else {
                return (boost::uint64_t)-1;
            }
        }

        boost::uint64_t Strategy::source_size()
        {
            boost::uint64_t total = 0;
            for (boost::uint32_t i = begin_segment_.pos; i < get_media()->segment_count(); i++) {
                ppbox::data::SegmentInfo seg_info;
                get_media()->segment_info(i, seg_info);
                total += seg_info.size;
            }
            return total;
        }

        boost::uint64_t Strategy::source_size_before(
            size_t pos)
        {
            assert(begin_segment_.pos <= pos);
            boost::uint64_t total = 0;
            if (pos > get_media()->segment_count()) {
                pos = get_media()->segment_count();
            }
            for (int i = begin_segment_.pos; i < pos; i++) {
                ppbox::data::SegmentInfo seg_info;
                get_media()->segment_info(i, seg_info);
                total += seg_info.size;
            }
            return total;
        }

        boost::uint64_t Strategy::source_time()
        {
            boost::uint64_t total = 0;
            for (int i = begin_segment_.pos; i < get_media()->segment_count(); i++) {
                ppbox::data::SegmentInfo seg_info;
                get_media()->segment_info(i, seg_info);
                total += seg_info.duration;
            }
            return total;
        }

        boost::uint64_t Strategy::source_time_before(
            size_t pos)
        {
            boost::uint64_t total = 0;
            if (pos > get_media()->segment_count()) {
                pos = get_media()->segment_count();
            }
            for (int i = begin_segment_.pos; i < pos; i++) {
                ppbox::data::SegmentInfo seg_info;
                get_media()->segment_info(i, seg_info);
                total += seg_info.duration;
            }
            return total;
        }

        boost::uint64_t Strategy::tree_size()
        {
            boost::uint64_t total = source_size();
            Strategy * item = (Strategy *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_size();
                    total += item->insert_delta_;
                }
                item = (Strategy *)item->next_sibling_;
            }
            return total;
        }

        boost::uint64_t Strategy::tree_time()
        {
            boost::uint64_t total = source_time();
            Strategy * item = (Strategy *)first_child_;
            while (item) {
                if (!item->skip_) {
                    total += item->tree_time();
                    item = (Strategy *)item->next_sibling_;
                }
            }
            return total;
        }

        boost::uint64_t Strategy::tree_size_before(
            Strategy * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total += source_size_before(child->insert_segment_) + child->insert_size_;
                Strategy * item = (Strategy *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_size();
                        total += item->insert_delta_; // 需要加入前半部分的数据才能正常播放
                    }
                    item = (Strategy *)item->prev_sibling_;
                }
            } else {
                total += tree_size();
            }
            return total;
        }

        boost::uint64_t Strategy::tree_time_before(
            Strategy * child)
        {
            boost::uint64_t total = 0;
            if (child) {
                total = source_time_before(child->insert_segment_) + child->insert_time_;
                Strategy * item = (Strategy *)child->prev_sibling_;
                while (item) {
                    if (!item->skip_) {
                        total += item->tree_time();
                        item = (Strategy *)item->prev_sibling_;
                    }
                }
            } else {
                total = tree_time();
            }
            return total;
        }

        boost::uint64_t Strategy::total_size_before(
            Strategy * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((Strategy *)parent_)->tree_size_before(this);
            }
            total += tree_size_before(child);
            return total;
        }

        boost::uint64_t Strategy::total_time_before(
            Strategy * child)
        {
            boost::uint64_t total = 0;
            if (parent_) {
                total += ((Strategy *)parent_)->total_time_before(this);
            }
            total += tree_time_before(child);
            return total;
        }
*/
    } // namespace demux
} // namespace ppbox

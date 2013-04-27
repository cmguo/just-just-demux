// DemuxStrategy.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/segment/DemuxStrategy.h"
#include "ppbox/data/base/SourceError.h"

#include <framework/system/LogicError.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
using namespace framework::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.DemuxStrategy", framework::logger::Debug);

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
                    ec = ppbox::data::source_error::no_more_segment;
                    return false; // 没有父节点了
                }
            } else {
                if (!media_.segment_info(pos.index, pos, ec)) {
                    --pos.index;
                    return false;
                }
                pos.byte_range.before_next();
                pos.time_range.before_next();
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

            return true;
        }

    } // namespace demux
} // namespace ppbox

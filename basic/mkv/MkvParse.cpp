// MkvParse.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/mkv/MkvParse.h"
#include "ppbox/demux/basic/mkv/MkvStream.h"
using namespace ppbox::demux::error;

using namespace ppbox::avformat::error;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.MkvParse", framework::logger::Debug)

namespace ppbox
{
    namespace demux
    {

        MkvParse::MkvParse(
            std::vector<MkvStream> & streams, 
            std::vector<size_t> & stream_map)
            : streams_(streams)
            , stream_map_(stream_map)
            , offset_(0)
            , end_(0)
            , cluster_end_(0)
            , offset_block_(0)
            , size_block_(0)
            , frame_(0)
            , in_group_(false)
            , duration_(0)
        {
        }

        void MkvParse::reset(
            boost::uint64_t off)
        {
            offset_ = off;
            end_ = 0;
            header_.clear();
            block_.sizes.clear();
            cluster_end_ = 0;
            offset_block_ = 0;
            size_block_ = 0;
            frame_= 0;
            in_group_ = false;
            duration_ = 0;
        }

        bool MkvParse::ready(
            ppbox::avformat::EBML_IArchive & ar, 
            boost::system::error_code & ec)
        {
            if (frame_ >= block_.sizes.size()) {
                block_.sizes.clear();
                if (!next_block(ar, ec))
                    return false;
                duration_ = (boost::int16_t)streams_[itrack()].sample_duration();
            }

            ar.seekg(offset_block_ + block_.sizes[frame_], std::ios::beg);
            if (ar) {
                return true;
            } else {
                ar.clear();
                ec = file_stream_error;
                return false;
            }
        }

        void MkvParse::next()
        {
            block_.TimeCode += duration_;
            offset_block_ += block_.sizes[frame_];
            ++frame_;
        }

        bool MkvParse::next_block(
            ppbox::avformat::EBML_IArchive & ar, 
            boost::system::error_code & ec)
        {
            using namespace ppbox::avformat;

            assert(ar);
            ar.seekg(offset_, std::ios::beg);
            assert(ar);

            while (true) {
                EBML_ElementHeader header;
                ar >> header;
                if (!ar) {
                    break;
                }
                boost::uint64_t data_offset = (boost::uint64_t)ar.tellg();
                boost::uint64_t data_end = data_offset + header.data_size();
                if (header.Id == MkvCluster::StaticId) {
                    if (!header_.empty() && !header_.Size.is_unknown()) {
                        LOG_WARN("[next_block] short cluster");
                    }
                    header_ = header;
                    end_ = cluster_end_ = data_end;
                    offset_ = data_offset;
                    continue;
                }
                if (header_.empty()) {
                    LOG_WARN("[next_block] out cluster element, id = " << (boost::uint32_t)header.Id);
                    ar.seekg(data_end, std::ios::beg);
                } else if (data_end > end_) {
                    if (in_group_) {
                        LOG_WARN("[next_block] excced group size");
                    } else {
                        LOG_WARN("[next_block] excced cluster size");
                    }
                    data_end = end_;
                    ar.seekg(data_end, std::ios::beg);
                } else if (in_group_) {
                    switch ((boost::uint32_t)header.Id) {
                        case MkvBlock::StaticId:
                            ar >> block_;
                            if (ar) {
                                offset_block_ = ar.tellg();
                                size_block_ = (boost::uint32_t)(data_end - offset_block_);
                                block_.compelte_load(size_block_);
                                frame_ = 0;
                            } else {
                                frame_ = block_.sizes.size();
                            }
                            break;
                        case 0x7B: // MkvBlockGroup::ReferenceBlock
                            (EBML_ElementHeader &)group_.ReferenceBlock = header;
                            group_.ReferenceBlock.load_value(ar);
                            break;
                        case 0x1B: // MkvBlockGroup::BlockDuration
                            (EBML_ElementHeader &)group_.BlockDuration = header;
                            group_.BlockDuration.load_value(ar);
                            break;
                        default:
                            LOG_WARN("[next_block] unknown element in group, id = " << (boost::uint32_t)header.Id 
                                << ", size = " << (boost::uint32_t)header.Size);
                            ar.seekg(data_end, std::ios::beg);
                            break;
                    }
                } else {
                    switch ((boost::uint32_t)header.Id) {
                        case 0x67: // MkvCluster::TimeCode
                            (EBML_ElementHeader &)cluster_.TimeCode = header;
                            cluster_.TimeCode.load_value(ar);
                            break;
                        case 0x2B: // MkvCluster::PrevSize
                            (EBML_ElementHeader &)cluster_.PrevSize = header;
                            cluster_.PrevSize.load_value(ar);
                            break;
                        case 0x27: // MkvCluster::v
                            (EBML_ElementHeader &)cluster_.Position = header;
                            cluster_.Position.load_value(ar);
                            break;
                        case MkvSimpleBlock::StaticId:
                            ar >> block_;
                            if (ar) {
                                offset_block_ = ar.tellg();
                                size_block_ = (boost::uint32_t)(data_end - offset_block_);
                                block_.compelte_load(size_block_);
                                frame_ = 0;
                            } else {
                                frame_ = block_.sizes.size();
                            }
                            break;
                        case MkvBlockGroup::StaticId:
                            in_group_ = true;
                            end_ = data_end;
                            data_end = data_offset;
                            break;
                        default:
                            LOG_WARN("[next_block] unknown element in cluster, id = " << (boost::uint32_t)header.Id 
                                << ", size = " << (boost::uint32_t)header.Size);
                            ar.seekg(data_end, std::ios::beg);
                            break;
                    }
                }
                if (ar) {
                    if (data_end == end_) {
                        if (in_group_) {
                            in_group_ = false;
                            end_ = cluster_end_;
                        }
                        if (data_end == end_) {
                            header_.clear();
                        }
                    }
                    offset_ = data_end;
                    if (!block_.sizes.empty()) {
                        break;
                    }
                } else {
                    break;
                }
            }
            if (ar) {
                assert(!block_.sizes.empty());
                return true;
            } else {
                if (ar.failed()) {
                    ec = bad_media_format;
                } else {
                    ec = file_stream_error;
                }
                ar.clear();
                ar.seekg(offset_, std::ios::beg);
                assert(ar);
                return false;
            }
        }

    } // namespace demux
} // namespace ppbox

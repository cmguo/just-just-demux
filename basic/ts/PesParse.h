// PesParse.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_

#include "ppbox/demux/basic/ts/PesStreamBuffer.h"
#include "ppbox/demux/basic/ts/PesAdtsSplitter.h"

#include <ppbox/avformat/ts/PesPacket.h>

#include <ppbox/avcodec/avc/AvcFrameType.h>

#include <utility>

namespace ppbox
{
    namespace demux
    {

        class PesParse
        {
        public:
            PesParse(
                boost::uint8_t stream_type)
                : stream_type_(stream_type)
                , size_(0)
                , left_(0)
            {
            }

        public:
            // 第一个bool表示是否完整
            // 第二个bool表示是否需要回退
            std::pair<bool, bool> add_packet(
                ppbox::avformat::TsPacket const & ts_pkt, 
                ppbox::avformat::TsIArchive & ar, 
                boost::system::error_code & ec)
            {
                boost::uint64_t offset = ar.tellg();
                boost::uint32_t size = ts_pkt.payload_size();
                if (ts_pkt.payload_uint_start_indicator == 1) {
                    if (payloads_.empty()) {
                        ar >> pkt_;
                        assert(ar);
                        size_ = left_ = pkt_.payload_length();
                        boost::uint64_t offset1 = ar.tellg();
                        size -= (boost::uint32_t)(offset1 - offset);
                        offset = offset1;
                    } else {
                        if (left_ != 0) {
                            LOG_WARN("[add_packet] payload size less than expect, size = " << size_ << ", less = " << left_);
                            size_ -= left_;
                            left_ = 0;
                        }
                        return std::make_pair(true, true);
                    }
                } else if (payloads_.empty()) {
                    LOG_WARN("[add_packet] payload with no pes come first");
                    return std::make_pair(false, false);
                }
                payloads_.push_back(ppbox::data::DataBlock(offset, size));
                if (left_ == 0) {
                    size_ += size;
                    return std::make_pair(false, false);
                } else {
                    if (left_ > size) {
                        left_ -= size;
                    } else if (left_ < size) {
                        LOG_WARN("[add_packet] payload size more than expect, size = " << size << ", more = " << size - left_);
                        size_ += size - left_;
                        left_ = 0;
                    } else {
                        left_ = 0;
                    }
                    // aac adts
                    if (stream_type_ == TsStreamType::iso_13818_7_audio) {
                        if (adts_splitter_.finish(ar, payloads_, size_ - left_)) {
                            size_ = adts_splitter_.size();
                            if (adts_splitter_.save_size() >= size) {
                                left_ += size;
                                adts_splitter_.pop_payload();
                                return std::make_pair(true, true);
                            } else {
                                return std::make_pair(true, false);
                            }
                        }
                        assert(left_);
                    }
                    return std::make_pair(left_ == 0, false);
                }
            }

            boost::uint64_t min_offset() const
            {
                return payloads_.empty() ? boost::uint64_t(-1) : payloads_.front().offset;
            }

            void adjust_offset(
                boost::uint64_t minus)
            {
                for (size_t i = 0; i < payloads_.size(); ++i) {
                    payloads_[i].offset -= minus;
                }
            }

            void clear(
                std::vector<ppbox::data::DataBlock> & payloads)
            {
                payloads.swap(payloads_);
                payloads_.clear();
                size_ = left_ = 0;
                if (stream_type_ == TsStreamType::iso_13818_7_audio) {
                    adts_splitter_.clear(payloads_);
                    size_ = adts_splitter_.save_size() + left_;
                }
            }

            boost::uint32_t size() const
            {
                return size_;
            }

            boost::uint64_t dts() const
            {
                if (pkt_.PTS_DTS_flags == 3) {
                    return time_dts_.transfer(pkt_.dts_bits.value());
                } else if (pkt_.PTS_DTS_flags == 2) {
                    return time_pts_.transfer(pkt_.pts_bits.value());
                } else {
                    return 0;
                }
            }

            boost::uint32_t cts_delta() const
            {
                if (pkt_.PTS_DTS_flags == 3) {
                    return (boost::uint32_t)(time_pts_.transfer(pkt_.pts_bits.value()) - time_dts_.transfer(pkt_.dts_bits.value()));
                } else {
                    return 0;
                }
            }

            void get_data(
                std::vector<boost::uint8_t> & data, 
                ppbox::avformat::TsIArchive & ar) const
            {
                using namespace framework::container;

                PesStreamBuffer buffer(*ar.rdbuf(), payloads_);
                TsIArchive ar1(buffer);

                data.resize(size_, 0);
                ar >> make_array(&data[0], size_);
                assert(ar);
            }

            bool is_sync_frame(
                ppbox::avformat::TsIArchive & ar) const
            {
                PesStreamBuffer buffer(*ar.rdbuf(), payloads_);
                std::basic_istream<boost::uint8_t> is(&buffer);
                if (stream_type_ == TsStreamType::iso_13818_video) {
                    avc_frame_.handle(is);
                    return avc_frame_.is_sync_frame();
                }
                return false;
            }

            void save_for_joint(
                ppbox::avformat::TsIArchive & ar)
            {
                is_sync_frame(ar);
            }

        private:
            ppbox::avformat::PesPacket pkt_;
            std::vector<ppbox::data::DataBlock> payloads_;
            boost::uint8_t stream_type_;
            boost::uint32_t size_;
            boost::uint32_t left_;
            mutable PesAdtsSplitter adts_splitter_;
            mutable ppbox::avcodec::AvcFrameType avc_frame_;
            mutable framework::system::LimitNumber<33> time_pts_;
            mutable framework::system::LimitNumber<33> time_dts_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_

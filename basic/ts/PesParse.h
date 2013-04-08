// PesParse.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_

#include <ppbox/avformat/ts/PesPacket.h>
#include <ppbox/avformat/codec/avc/AvcNaluHelper.h>
#include <ppbox/avformat/codec/avc/AvcNalu.h>

#include <utility>

namespace ppbox
{
    namespace demux
    {

        class PesParse
        {
        public:
            PesParse()
                : size_(0)
                , left_(0)
            {
                frame_offset_[0] = frame_offset_[1] = 0;
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
                        return std::make_pair(false, false);
                    }
                    if (left_ < size) {
                        LOG_WARN("[add_packet] payload size more than expect, size = " << size << ", more = " << size - left_);
                        size_ += size - left_;
                        left_ = 0;
                    }
                    return std::make_pair(true, false);
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
                size_ = left_ = 0;
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

                data.resize(size_, 0);
                boost::uint32_t read_offset = 0;
                for (size_t i = 0; i < payloads_.size(); ++i) {
                    ar.seekg(payloads_[i].offset, std::ios::beg);
                    assert(ar);
                    ar >> make_array(&data[read_offset], payloads_[i].size);
                    assert(ar);
                    read_offset += payloads_[i].size;
                }
            }

            bool is_sync_frame(
                ppbox::avformat::TsIArchive & ar) const
            {
                using namespace ppbox::avformat;
                using namespace framework::container;

                if (frame_offset_[0] > 0) {
                    boost::uint8_t data[5];
                    boost::uint32_t frame_offset = frame_offset_[0];
                    boost::uint32_t read_size = 0;
                    for (size_t i = 0; i < payloads_.size() && read_size < 5; ++i) {
                        if (frame_offset < payloads_[i].size) {
                            ar.seekg(payloads_[i].offset + frame_offset, std::ios::beg);
                            assert(ar);
                            boost::uint32_t read_size2 = 5 - read_size;
                            if (frame_offset + read_size2 > payloads_[0].size) {
                                read_size2 = payloads_[i].size - frame_offset;
                            }
                            ar >> make_array(data + read_size, read_size2);
                            read_size += read_size2;
                            frame_offset = 0;
                        } else {
                            frame_offset -= payloads_[i].size;
                        }
                    }
                    if (read_size == 5 
                        && *(boost::uint32_t *)data == MAKE_FOURC_TYPE(0, 0, 0, 1)) {
                            NaluHeader h(data[4]);
                            if (h.nal_unit_type == 1) {
                                return false;
                            } else if (h.nal_unit_type == 5) {
                                return true;
                            }
                    }
                }

                if (frame_offset_[1] > 0) {
                    return frame_offset_[1] == 1;
                }

                std::vector<boost::uint8_t> data;
                get_data(data, ar);

                AvcNaluHelper helper;
                boost::uint8_t frame_type = helper.get_frame_type_from_stream(data, frame_offset_);
                if (frame_type == 1) {
                    return false;
                } else if (frame_type == 5) {
                    return true;
                } else {
                    return false;
                }
            }

            void save_for_joint(
                ppbox::avformat::TsIArchive & ar)
            {
                frame_offset_[1] = is_sync_frame(ar) ? 1 : 2;
                frame_offset_[0] = 0;
            }

        private:
            ppbox::avformat::PesPacket pkt_;
            std::vector<ppbox::data::DataBlock> payloads_;
            boost::uint32_t size_;
            boost::uint32_t left_;
            mutable boost::uint32_t frame_offset_[2];
            mutable framework::system::LimitNumber<33> time_pts_;
            mutable framework::system::LimitNumber<33> time_dts_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_PES_PARSE_H_

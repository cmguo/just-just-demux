// AsfParse.h

#ifndef _JUST_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_
#define _JUST_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_

#include <just/avformat/asf/AsfObjectType.h>
#include <just/data/base/DataBlock.h>

#include <framework/system/LimitNumber.h>

#include <utility>

namespace just
{
    namespace demux
    {

        class AsfParse
        {
        public:
            AsfParse()
                : next_object_offset_(0)
                , is_discontinuity_(false)
            {
            }

        public:
            void add_packet(
                just::avformat::AsfPacket const & pkt)
            {
            }

            bool add_payload(
                just::avformat::AsfParseContext const & context, 
                just::avformat::AsfPayloadHeader const & payload)
            {
                if (!payloads_.empty() && payload.MediaObjNum != payload_.MediaObjNum) {
                    next_object_offset_ = 0;
                    is_discontinuity_ = true;
                    payloads_.clear();
                }
                if (payload.OffsetIntoMediaObj != next_object_offset_) {
                    // Payload not continue
                    next_object_offset_ = 0;
                    is_discontinuity_ = true;
                    payloads_.clear();
                    return false;
                }
                if (payloads_.empty()) {
                    payload_ = payload;
                }
                payloads_.push_back(just::data::DataBlock(context.payload_data_offset, payload.PayloadLength));
                next_object_offset_ += payload.PayloadLength;
                return finish();
            }

            bool finish() const
            {
                return next_object_offset_ == payload_.MediaObjectSize;
            }

            void clear()
            {
                next_object_offset_ = 0;
                is_discontinuity_ = false;
                payloads_.clear();
            }

            void clear(
                std::vector<just::data::DataBlock> & payloads)
            {
                payloads.swap(payloads_);
                clear();
            }

            boost::uint32_t size() const
            {
                return payload_.MediaObjectSize;
            }

            boost::uint64_t dts() const
            {
                return timestamp.transfer(payload_.PresTime);
            }

            boost::uint32_t cts_delta() const
            {
                return 0;
            }

            bool is_sync_frame() const
            {
                return payload_.KeyFrameBit == 1;
            }

            bool is_discontinuity() const
            {
                return is_discontinuity_;
            }

            std::vector<just::data::DataBlock> const &  payloads() const
            {
                return payloads_;
            }

        private:
            just::avformat::AsfPayloadHeader payload_;
            boost::uint64_t next_object_offset_; // not offset in file, just offset of this object
            bool is_discontinuity_;
            std::vector<just::data::DataBlock> payloads_;
            mutable framework::system::LimitNumber<32> timestamp;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_

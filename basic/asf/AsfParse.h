// AsfParse.h

#ifndef _PPBOX_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_
#define _PPBOX_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_

#include <ppbox/avformat/asf/AsfObjectType.h>
#include <ppbox/data/base/DataBlock.h>

#include <framework/system/LimitNumber.h>

#include <utility>

namespace ppbox
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
                ppbox::avformat::AsfPacket const & pkt)
            {
            }

            bool add_payload(
                ppbox::avformat::AsfParseContext const & context, 
                ppbox::avformat::AsfPayloadHeader const & payload)
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
                payloads_.push_back(ppbox::data::DataBlock(context.payload_data_offset, payload.PayloadLength));
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
                std::vector<ppbox::data::DataBlock> & payloads)
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

            std::vector<ppbox::data::DataBlock> const &  payloads() const
            {
                return payloads_;
            }

        private:
            ppbox::avformat::AsfPayloadHeader payload_;
            boost::uint64_t next_object_offset_; // not offset in file, just offset of this object
            bool is_discontinuity_;
            std::vector<ppbox::data::DataBlock> payloads_;
            mutable framework::system::LimitNumber<32> timestamp;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_ASF_ASF_PES_PARSE_H_

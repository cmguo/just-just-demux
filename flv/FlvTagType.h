// FlvTagType.h

#ifndef _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_
#define _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

#include "ppbox/demux/flv/FlvFormat.h"
#include "ppbox/demux/flv/FlvDataType.h"

namespace ppbox
{
    namespace demux
    {

        struct FlvHeader
        {
            boost::uint8_t Signature1;
            boost::uint8_t Signature2;
            boost::uint8_t Signature3;
            boost::uint8_t Version;
            boost::uint8_t TypeFlagsReserved : 5;
            boost::uint8_t TypeFlagsAudio : 1;
            boost::uint8_t TypeFlagsReserved2 : 1;
            boost::uint8_t TypeFlagsVideo : 1;
            boost::uint32_t DataOffset;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                boost::uint8_t temp;
                ar & Signature1;
                ar & Signature2;
                ar & Signature3;
                ar & Version;
                ar & temp;
                ar & DataOffset;
                TypeFlagsReserved = (temp & 0xF8) >> 3;
                TypeFlagsAudio = (temp & 0x04) >> 2;
                TypeFlagsReserved2 = (temp & 0x02) >> 1;
                TypeFlagsVideo = (temp & 0x01);
            }
        };

        struct FlvAudioTag
        {
            boost::uint8_t SoundFormat : 4;
            boost::uint8_t SoundRate : 2;
            boost::uint8_t SoundSize : 1;
            boost::uint8_t SoundType : 1;
            boost::uint8_t AACPacketType;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                boost::uint8_t temp;
                ar & temp;
                SoundFormat = (temp & 0xF0) >> 4;
                SoundRate = (temp & 0x0C) >> 2;
                SoundSize = (temp & 0x02) >> 1;
                SoundType = temp & 0x01;
                ar & AACPacketType;
            }
        };

        struct FlvVideoTag
        {
            boost::uint8_t FrameType : 4;
            boost::uint8_t CodecID : 4;
            boost::uint8_t AVCPacketType;
            boost::uint32_t CompositionTime;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                boost::uint8_t temp;
                ar & temp;
                FrameType = (temp & 0xF0) >> 4;
                CodecID  = (temp & 0x0F);
                ar & AVCPacketType;
                boost::uint8_t temp2[3];
                ar & framework::container::make_array(temp2);
                CompositionTime = BigEndianUint24ToHostUint32(temp2);
            }
        };

        struct FlvDataTag
        {
            FlvDataValue Name;
            FlvDataValue Value;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & Name;
                ar & Value;
            }
        };

        struct FlvTag
        {
            FlvTag()
            {
            }

            boost::uint32_t PreTagSize;
            boost::uint8_t  Reserved : 2;
            boost::uint8_t  Filter : 1;
            boost::uint8_t  Type : 5;
            boost::uint32_t DataSize;
            boost::uint32_t Timestamp;
            boost::uint32_t StreamID;
            FlvAudioTag AudioTag;
            FlvVideoTag VideoTag;
            FlvDataTag DataTag;
            boost::uint64_t data_offset;

            boost::uint8_t input[3];
            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & PreTagSize;
                boost::uint8_t temp1 = 0, temp2 = 0, temp3 = 0;
                ar & temp1;
                Reserved = (temp1 & 0xC0) >> 6;
                Filter   = (temp1 & 0x20) >> 5;
                Type  = temp1 & 0x1F;
                ar & temp1 & temp2 & temp3;
                input[0] = temp1; input[1] = temp2; input[2] = temp3;
                DataSize = BigEndianUint24ToHostUint32(input);
                //ar & framework::container::make_array(input);
                if (ar && DataSize == 0) {
                    ar.fail();
                }
                ar & temp1 & temp2 & temp3;
                input[0] = temp1; input[1] = temp2; input[2] = temp3;
                Timestamp = BigEndianUint24ToHostUint32(input);
                ar & temp1;
                boost::uint32_t TimestampExtended = temp1 << 24;
                Timestamp += TimestampExtended;
                //ar & framework::container::make_array(input);
                ar & temp1 & temp2 & temp3;
                input[0] = temp1; input[1] = temp2; input[2] = temp3;
                StreamID = BigEndianUint24ToHostUint32(input);
                data_offset = ar.tellg();
                if (Type == TagType::FLV_TAG_TYPE_AUDIO) {
                    ar & AudioTag;
                } else if (Type == TagType::FLV_TAG_TYPE_VIDEO) {
                    ar & VideoTag;
                } else if (Type == TagType::FLV_TAG_TYPE_META) {
                    ar & DataTag;
                }
                if (DataSize + data_offset < ar.tellg()) {
                    ar.fail();
                } else {
                    DataSize = DataSize + data_offset - ar.tellg();
                    data_offset = ar.tellg();
                }
                ar.seekg(DataSize, std::ios_base::cur);
            }
        };

    }
}

#endif // _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

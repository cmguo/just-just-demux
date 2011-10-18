// FlvTagType.h

#ifndef _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_
#define _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

#include <util/archive/BigEndianBinaryIArchive.h>
#include <util/serialization/stl/vector.h>

#include <framework/timer/TickCounter.h>
#include <iostream>

namespace ppbox
{
    namespace demux
    {

        typedef boost::system::error_code FLV_ERROR;
        typedef util::archive::BigEndianBinaryIArchive<boost::uint8_t> FLVArchive;

        /*return 1 : little-endian, return 0:big-endian*/
        static bool endian(void)
        {
            union
            {
                boost::uint32_t a;
                boost::uint8_t  b;
            } c;
            c.a = 1;
            return (c.b == 1);
        }

        static boost::uint32_t BigEndianUint24ToHostUint32(
            boost::uint8_t in[3])
        {
            boost::uint32_t value = 0;
            boost::uint8_t *p = (boost::uint8_t*)&value;
            if (endian()) {
                *p = in[2];
                *(p+1) = in[1];
                *(p+2) = in[0];
            } else {
                *p = 0;
                *(p+1) = in[0];
                *(p+2) = in[1];
                *(p+3) = in[2];
            }
            return value;
        }

        //template <typename Archive>
        //void serialize_length(
        //    Archive & ar, 
        //    boost::uint32_t & t, 
        //    size_t m)
        //{
        //    t = 0;
        //    switch (m) {
        //        case 0:
        //            break;
        //        case 1:
        //            {
        //                boost::uint8_t tt;
        //                ar & tt;
        //                t = tt;
        //            }
        //            break;
        //        case 2:
        //            {
        //                boost::uint16_t tt;
        //                ar & tt;
        //                t = tt;
        //            }
        //            break;
        //        case 3:
        //            ar & t;
        //            break;
        //        default:
        //            assert(0);
        //            break;
        //    }
        //}

        struct FlvHeader
        {
            boost::uint8_t Signature1;
            boost::uint8_t Signature2;
            boost::uint8_t Signature3;
            boost::uint8_t Version;
            boost::uint8_t Property;
            boost::uint32_t DataOffset;
            boost::uint32_t Pre0Tagsize;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & Signature1;
                ar & Signature2;
                ar & Signature3;
                ar & Version;
                ar & Property;
                ar & DataOffset;
                ar & Pre0Tagsize;
            }
        };

        struct FlvTag
        {
            FlvTag(std::vector<boost::uint8_t> & buf)
                : TagData(buf)
            {
            }

            boost::uint8_t  Reserved : 2;
            boost::uint8_t  Filter : 1;
            boost::uint8_t  TagType : 5;
            boost::uint32_t DataSize;
            boost::uint32_t Timestamp;
            boost::uint32_t StreamID;
            std::vector<boost::uint8_t> & TagData;
            boost::uint32_t PreTagSize;

            boost::uint8_t input[3];
            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                boost::uint8_t temp1 = 0, temp2 = 0, temp3 = 0;
                ar & temp1;
                Reserved = (temp1 & 0xC0) >> 6;
                Filter   = (temp1 & 0x20) >> 5;
                TagType  = temp1 & 0x1F;
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
                framework::timer::TickCounter tc;
                //util::serialization::serialize_collection(ar, TagData, DataSize);
                TagData.resize(DataSize);
                if (DataSize > 0) {
                    boost::uint8_t * data_buf = &TagData.at(0);
                    ar & framework::container::make_array(data_buf, DataSize);
                }
                ar & PreTagSize;
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

        //struct FlvVideoTag
        //{
        //    boost::uint8_t FrameType : 4;
        //    boost::uint8_t CodecID : 4;
        //    boost::uint8_t AVCPacketType;
        //    boost::uint32_t CompositionTime;

        //    template <typename Archive>
        //    void serialize(
        //        Archive & ar)
        //    {
        //        boost::uint8_t temp;
        //        ar & temp;
        //        FrameType = (temp & 0xF0) >> 4;
        //        CodecID  = (temp & 0x0F);
        //        ar & AVCPacketType;
        //        boost::uint8_t temp[3];
        //        ar & framework::container::make_array(temp);
        //        CompositionTime = BigEndianUint24ToHostUint32(temp);
        //    }
        //};

        struct FlvPreTagSizde
        {
            boost::uint32_t size;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & size;
            }
        };
    }
}

#endif // _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

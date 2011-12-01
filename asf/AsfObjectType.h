// AsfObjectType.h

#ifndef _PPBOX_DEMUX_ASF_ASF_OBJECT_TYPE_H_
#define _PPBOX_DEMUX_ASF_ASF_OBJECT_TYPE_H_

#include <util/archive/LittleEndianBinaryIArchive.h>
#include <util/serialization/stl/vector.h>

#include <framework/string/Uuid.h>

namespace framework
{
    namespace string
    {

        template <typename Archive>
        void serialize(Archive & ar, Uuid & t)
        {
            util::serialization::split_free(ar, t);
        }

        template <typename Archive>
        void load(Archive & ar, Uuid & t)
        {
            UUID guid;
            ar & guid.Data1
                & guid.Data2
                & guid.Data3
                & framework::container::make_array(guid.Data4, sizeof(guid.Data4));
            if (ar)
                t.assign(guid);
        };

        template <typename Archive>
        void save(Archive & ar, Uuid const & t)
        {
            UUID const & guid = t.data();
            ar & guid.Data1
                & guid.Data2
                & guid.Data3
                & framework::container::make_array(guid.Data4, sizeof(guid.Data4));
        }
    }
}

namespace ppbox
{
    namespace demux
    {

        typedef  boost::system::error_code ASF_ERROR; 
        typedef util::archive::LittleEndianBinaryIArchive<boost::uint8_t> ASFArchive;

        typedef framework::string::Uuid AsfUuid;
        typedef framework::string::UUID ASFUUID;

        template <typename Archive>
        void serialize_length(Archive & ar, boost::uint32_t & t, size_t m)
        {
            t = 0;
            switch (m) {
                case 0:
                    break;
                case 1:
                    {
                        boost::uint8_t tt;
                        ar & tt;
                        t = tt;
                    }
                    break;
                case 2:
                    {
                        boost::uint16_t tt;
                        ar & tt;
                        t = tt;
                    }
                    break;
                case 3:
                    ar & t;
                    break;
                default:
                    assert(0);
                    break;
            }
        }

        struct ASF_Header_Object
        {
            AsfUuid guid;
            boost::uint64_t ObjectSize;
            boost::uint32_t NumOfHeaderObject;
            boost::uint8_t Reserved1;
            boost::uint8_t Reserver2;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & guid
                    & ObjectSize
                    & NumOfHeaderObject
                    & Reserved1
                    & Reserver2;
            }
        };

        struct ASF_Object_Header
        {
            AsfUuid ObjectId;
            boost::uint64_t ObjLength;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & ObjectId
                    & ObjLength;
            }
        };

        struct PropertiesFlag
        {
            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN 
                    boost::uint32_t BroadcastFlag : 1;
                    boost::uint32_t SeekableFlag : 1;
                    boost::uint32_t Reserved : 30;
#else 
                    boost::uint32_t Reserved : 30;
                    boost::uint32_t SeekableFlag : 1;
                    boost::uint32_t BroadcastFlag : 1;
#endif
                };
                boost::uint32_t flag;
            };

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & flag;
            }
        };

        struct ASF_File_Properties_Object_Data
        {
            AsfUuid FileId;
            boost::uint64_t FileSize;
            boost::uint64_t CreationDate;
            boost::uint64_t DataPacketsCount;
            boost::uint64_t PlayDuration;
            boost::uint64_t SendDuration;
            boost::uint64_t Preroll;
            struct PropertiesFlag Flag;
            boost::uint32_t MinimumDataPacketSize;
            boost::uint32_t MaximumDataPacketSize;
            boost::uint32_t MaximumBitrate;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & FileId
                    & FileSize
                    & CreationDate
                    & DataPacketsCount
                    & PlayDuration
                    & SendDuration
                    & Preroll
                    & Flag
                    & MinimumDataPacketSize
                    & MaximumDataPacketSize
                    & MaximumBitrate;
            }
        };

        struct StreamProperFlag
        {
            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN
                    boost::uint16_t StreamNumber : 7;
                    boost::uint16_t Reserved : 8;
                    boost::uint16_t EncryptedContentFlag : 1;
#else 
                    boost::uint16_t EncryptedContentFlag : 1;
                    boost::uint16_t Reserved : 8;
                    boost::uint16_t StreamNumber : 7;
#endif
                };
                boost::uint16_t flag;
            };

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & flag;
            }
        };

        struct ASF_Audio_Media_Type
        {
            boost::uint16_t CodecId;
            boost::uint16_t NumberOfChannels;
            boost::uint32_t SamplesPerSecond;
            boost::uint32_t AverageNumberOfBytesPerSecond;
            boost::uint16_t BlockAlignment;
            boost::uint16_t BitsPerSample;
            boost::uint16_t CodecSpecificDataSize;
            std::vector<boost::uint8_t> CodecSpecificData;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & CodecId
                    & NumberOfChannels
                    & SamplesPerSecond
                    & AverageNumberOfBytesPerSecond
                    & BlockAlignment
                    & BitsPerSample
                    & CodecSpecificDataSize;
                util::serialization::serialize_collection(ar, CodecSpecificData, CodecSpecificDataSize);
            }
        };

        struct ASF_Video_Media_Type
        {
            boost::uint32_t EncodeImageWidth;
            boost::uint32_t EncodeImageHeight;
            boost::uint8_t ReservedFlags;
            boost::uint16_t FormatDataSize;
            struct Format_Data {
                boost::uint32_t FormatDataSize;
                boost::uint32_t ImageWidth;
                boost::uint32_t ImageHeight;
                boost::uint16_t Reserved;
                boost::uint16_t BitsPerPixelCount;
                boost::uint32_t CompressionID;
                boost::uint32_t ImageSize;
                boost::uint32_t HorizontalPixelsPerMeter;
                boost::uint32_t VerticalPixelsPerMeter;
                boost::uint32_t ColorsUsedCount;
                boost::uint32_t ImportantColorsCount;
                std::vector<boost::uint8_t> CodecSpecificData;

                template <typename Archive>
                void serialize(Archive & ar)
                {
                    ar & FormatDataSize
                        & ImageWidth
                        & ImageHeight
                        & Reserved
                        & BitsPerPixelCount
                        & framework::container::make_array((boost::uint8_t *)&CompressionID, sizeof(CompressionID))
                        & ImageSize
                        & HorizontalPixelsPerMeter
                        & VerticalPixelsPerMeter
                        & ColorsUsedCount
                        & ImportantColorsCount;
                    util::serialization::serialize_collection(ar, CodecSpecificData, FormatDataSize - 40);
                }
            } FormatData;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & EncodeImageWidth
                    & EncodeImageHeight
                    & ReservedFlags
                    & FormatDataSize
                    & FormatData;
            }
        };

        struct ASF_Stream_Properties_Object_Data
        {
            AsfUuid StreamType;
            AsfUuid ErrorCorrectionType;
            boost::uint64_t TimeOffset;
            boost::uint32_t TypeSpecificDataLength;
            boost::uint32_t ErrorCorrectionDataLength;
            struct StreamProperFlag Flag;
            boost::uint32_t Reserved;
            std::vector<boost::uint8_t> TypeSpecificData;
            std::vector<boost::uint8_t> ErrorCorrectionData;
            ASF_Video_Media_Type Video_Media_Type;
            ASF_Audio_Media_Type Audio_Media_Type;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ASFUUID ASF_Audio_Media = {
                    0xF8699E40,0x5B4D,0x11CF,{0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B}};
                ASFUUID ASF_Video_Media = {
                    0xBC19EFC0,0x5B4D,0x11CF,{0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B}};

                ar & StreamType
                    & ErrorCorrectionType
                    & TimeOffset
                    & TypeSpecificDataLength
                    & ErrorCorrectionDataLength
                    & Flag
                    & Reserved;
                if (TypeSpecificDataLength) {
                    util::serialization::serialize_collection(ar, TypeSpecificData, TypeSpecificDataLength);
                }
                // 回退并解析音视频MediaType
                if (ar) {
                    boost::uint32_t offset1 = ar.tellg();
                    ar.seekg(-(int)TypeSpecificDataLength, std::ios::cur);
                    if (ASF_Video_Media == StreamType) {
                        ar & Video_Media_Type;
                    } else if (ASF_Audio_Media == StreamType) {
                        ar & Audio_Media_Type;
                    } else {
                        ar.seekg(TypeSpecificDataLength, std::ios::cur);
                    }
                    boost::uint32_t offset2 = ar.tellg();
                    (void)offset1;
                    (void)offset2;
                    assert(!ar || offset2 == offset1);
                }
                if (ErrorCorrectionDataLength > 0) {
                    util::serialization::serialize_collection(ar, ErrorCorrectionData, ErrorCorrectionDataLength);
                }
            }
        };

        struct ErrorCorrrectionData
        {
            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN
                    boost::uint8_t ErrorCorrectionDataLength : 4;
                    boost::uint8_t OpaqueDataPresent : 1;
                    boost::uint8_t ErrorCorrectionLengthType : 2;
                    boost::uint8_t ErrorCorrectionPresent : 1;
#else 
                    boost::uint8_t ErrorCorrectionPresent : 1;
                    boost::uint8_t ErrorCorrectionLengthType : 2;
                    boost::uint8_t OpaqueDataPresent : 1;
                    boost::uint8_t ErrorCorrectionDataLength : 4;
#endif
                };
                boost::uint8_t flag;
            };
            std::vector<boost::uint8_t> Data;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & flag;
                assert(!ar || (ErrorCorrectionPresent && ErrorCorrectionLengthType == 0));
                if (ar && (ErrorCorrectionPresent == 0 || ErrorCorrectionLengthType != 0)) {
                    ar.fail();
                }
                util::serialization::serialize_collection(ar, Data, ErrorCorrectionDataLength);
            }
        };

        struct PayLoadParsingInformation
        {
            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN
                    boost::uint8_t MultiplePayloadsPresent : 1;
                    boost::uint8_t SequenceType : 2;
                    boost::uint8_t PaddingLengthType : 2;
                    boost::uint8_t PacketLengthType : 2;
                    boost::uint8_t ErrorCorrectionPresent : 1;
#else 
                    boost::uint8_t ErrorCorrectionPresent : 1;
                    boost::uint8_t PacketLengthType : 2;
                    boost::uint8_t PaddingLengthType : 2;
                    boost::uint8_t SequenceType : 2;
                    boost::uint8_t MultiplePayloadsPresent : 1;
#endif
                };
                boost::uint8_t flag1;
            };

            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN
                    boost::uint8_t ReplicatedDataLengthType : 2;
                    boost::uint8_t OffsetIntoMOLType : 2;
                    boost::uint8_t MediaObjNumType : 2;
                    boost::uint8_t StreamNumLengthType : 2;
#else 
                    boost::uint8_t StreamNumLengthType : 2;
                    boost::uint8_t MediaObjNumType : 2;
                    boost::uint8_t OffsetIntoMOLType : 2;
                    boost::uint8_t ReplicatedDataLengthType : 2;
#endif
                };
                boost::uint8_t flag2;
            };
            boost::uint32_t PacketLenth;
            boost::uint32_t Sequence;
            boost::uint32_t PaddingLength;
            boost::uint32_t SendTime;
            boost::uint16_t Duration;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & flag1;
                assert(!ar || MultiplePayloadsPresent || PacketLengthType);
                if (ar && !MultiplePayloadsPresent && PacketLengthType == 0) {
                    ar.fail();
                }
                ar & flag2;
                serialize_length(ar, PacketLenth, PacketLengthType);
                serialize_length(ar, Sequence, SequenceType);
                serialize_length(ar, PaddingLength, PaddingLengthType);
                ar  & SendTime & Duration;
            }
        };

        struct ASF_Data_Object
        {
            AsfUuid ObjectId;
            boost::uint64_t ObjLength;
            AsfUuid FileId;
            boost::uint64_t TotalDataPackets;
            boost::uint16_t Reaerved;

            template <typename Archive>
            void serialize(Archive & ar)
            {
                ar & ObjectId
                    & ObjLength
                    & FileId
                    & TotalDataPackets
                    & Reaerved;
            }
        };

        struct ASF_Packet
        {
            //boost::uint32_t MaximumDataPacketSize;
            ErrorCorrrectionData ErrorCorrorectionInfo;
            PayLoadParsingInformation PayLoadParseInfo;

            union {
                struct {
#ifdef   BOOST_LITTLE_ENDIAN
                    boost::uint8_t PayloadNum : 6;
                    boost::uint8_t PayloadLengthType : 2;
#else 
                    boost::uint8_t PayloadLengthType : 2;
                    boost::uint8_t PayloadNum : 6;
#endif
                };
                boost::uint8_t flag;
            };
            boost::uint32_t PayloadEnd;

            ASF_Packet(
                boost::uint32_t const & MaximumDataPacketSize)
                : MaximumDataPacketSize(MaximumDataPacketSize)
            {
            }

            template <typename Archive>
            void serialize(Archive & ar)
            {
                boost::uint32_t start_offset = ar.tellg();
                ar & ErrorCorrorectionInfo
                    & PayLoadParseInfo;

                if (PayLoadParseInfo.MultiplePayloadsPresent) {
                    ar & flag;
                    PayloadEnd = start_offset + MaximumDataPacketSize - PayLoadParseInfo.PaddingLength;
                    assert(!ar || PayLoadParseInfo.PacketLenth == 0);
                    if (ar && PayLoadParseInfo.PacketLenth != 0) {
                        ar.fail();
                    }
                } else {
                    assert(!ar || PayLoadParseInfo.PacketLenth == MaximumDataPacketSize);
                    if (ar && PayLoadParseInfo.PacketLenth != MaximumDataPacketSize) {
                        ar.fail();
                    }
                    PayloadNum = 1;
                    PayloadLengthType = 0;
                    boost::uint32_t end_offset = ar.tellg();
                    assert(!ar || start_offset + PayLoadParseInfo.PacketLenth > end_offset + PayLoadParseInfo.PaddingLength);
                    if (ar && start_offset + PayLoadParseInfo.PacketLenth <= end_offset + PayLoadParseInfo.PaddingLength) {
                        ar.fail();
                    }
                    PayloadEnd = start_offset + PayLoadParseInfo.PacketLenth - PayLoadParseInfo.PaddingLength;
                }
            }
        private:
            boost::uint32_t const & MaximumDataPacketSize;
        };

        struct ASF_PayloadHeader
        {
            boost::uint8_t StreamNum : 7;
            boost::uint8_t KeyFrameBit : 1;
            boost::uint32_t MediaObjNum;
            boost::uint32_t OffsetIntoMediaObj;
            boost::uint32_t ReplicatedDataLen;
            boost::uint32_t MediaObjectSize;
            boost::uint32_t PresTime;
            std::vector<boost::uint8_t> ReplicateData;
            boost::uint32_t PayloadLength;
            boost::uint32_t data_offset;

            ASF_PayloadHeader()
                : packet_(NULL)
            {
            }

            void set_packet(
                ASF_Packet const & packet)
            {
                packet_ = &packet;
            }

            template <typename Archive>
            void serialize(Archive & ar)
            {
                boost::uint8_t temp;
                ar & temp;
                StreamNum = temp & 0x7f;
                KeyFrameBit = (temp & 0x80) >> 7;
                serialize_length(ar, MediaObjNum, packet_->PayLoadParseInfo.MediaObjNumType);
                serialize_length(ar, OffsetIntoMediaObj, packet_->PayLoadParseInfo.OffsetIntoMOLType);
                serialize_length(ar, ReplicatedDataLen, packet_->PayLoadParseInfo.ReplicatedDataLengthType);
                // 不考虑压缩的情形
                assert(!ar || ReplicatedDataLen >= 8);
                boost::uint32_t start_offset = ar.tellg();
                if (ar && (ReplicatedDataLen < 8 || start_offset + ReplicatedDataLen >= packet_->PayloadEnd)) {
                    ar.fail();
                }
                ar & MediaObjectSize;
                ar & PresTime;
                // already read 8 bytes
                if (ReplicatedDataLen > 8)
                    util::serialization::serialize_collection(ar, ReplicateData, ReplicatedDataLen - 8);
                if (packet_->PayLoadParseInfo.MultiplePayloadsPresent) {
                    serialize_length(ar, PayloadLength, packet_->PayloadLengthType);
                    data_offset = ar.tellg();
                    if (ar && PayloadLength + data_offset > packet_->PayloadEnd) {
                        ar.fail();
                    }
                } else {
                    data_offset = ar.tellg();
                    PayloadLength = packet_->PayloadEnd - data_offset;
                }
                ar.seekg(PayloadLength, std::ios_base::cur);
            }

        private:
            ASF_Packet const * packet_;
        };

    }
}

#endif // _PPBOX_DEMUX_ASF_ASF_OBJECT_TYPE_H_

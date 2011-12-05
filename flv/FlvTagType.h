// FlvTagType.h

#ifndef _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_
#define _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

#include "ppbox/demux/flv/FlvFormat.h"
#include "ppbox/demux/flv/FlvDataType.h"

#include <util/serialization/NumberBits24.h>

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
            union {
                struct {
#ifdef   BOOST_BIG_ENDIAN
                    boost::uint8_t TypeFlagsReserved : 5;
                    boost::uint8_t TypeFlagsAudio : 1;
                    boost::uint8_t TypeFlagsReserved2 : 1;
                    boost::uint8_t TypeFlagsVideo : 1;
#else 
                    boost::uint8_t TypeFlagsVideo : 1;
                    boost::uint8_t TypeFlagsReserved2 : 1;
                    boost::uint8_t TypeFlagsAudio : 1;
                    boost::uint8_t TypeFlagsReserved : 5;
#endif
                };
                boost::uint8_t flag;
            };
            boost::uint32_t DataOffset;
            boost::uint32_t PreTagSize;

            FlvHeader()
                : Signature1('F')
                , Signature2('L')
                , Signature3('V')
                , Version(1)
                , flag(5) // TypeFlagsReserved = 0, TypeFlagsAudio = 1, TypeFlagsReserved2 = 0, TypeFlagsVideo = 0
                , DataOffset(9)
                , PreTagSize(0)
            {
            }

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & Signature1;
                ar & Signature2;
                ar & Signature3;
                if (!(Signature1 == 'F' && Signature2 == 'L' && Signature3 == 'V')) {
                    ar.fail();
                }
                ar & Version;
                ar & flag;
                ar & DataOffset;
                ar & PreTagSize;
            }
        };

        struct FlvAudioTagHeader
        {
            union {
                struct {
#ifdef   BOOST_BIG_ENDIAN
                    boost::uint8_t SoundFormat : 4;
                    boost::uint8_t SoundRate : 2;
                    boost::uint8_t SoundSize : 1;
                    boost::uint8_t SoundType : 1;
#else 
                    boost::uint8_t SoundType : 1;
                    boost::uint8_t SoundSize : 1;
                    boost::uint8_t SoundRate : 2;
                    boost::uint8_t SoundFormat : 4;
#endif
                };
                boost::uint8_t flag;
            };
            boost::uint8_t AACPacketType;

            FlvAudioTagHeader()
                : flag(0) // SoundFormat = 0, SoundRate = 0, SoundSize = 0, SoundType = 0
                , AACPacketType(0)
            {
            }

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & flag;
                if (SoundFormat == SoundCodec::FLV_CODECID_AAC)
                    ar & AACPacketType;
                else
                    AACPacketType = 1;
            }
        };

        struct FlvVideoTagHeader
        {
            union {
                struct {
#ifdef   BOOST_BIG_ENDIAN
                    boost::uint8_t FrameType : 4;
                    boost::uint8_t CodecID : 4;
#else 
                    boost::uint8_t CodecID : 4;
                    boost::uint8_t FrameType : 4;
#endif
                };
                boost::uint8_t flag;
            };
            boost::uint8_t AVCPacketType;
            framework::system::UInt24 CompositionTime;

            FlvVideoTagHeader()
                : flag(0) // FrameType = 0, CodecID = 0
                , AVCPacketType(0)
                , CompositionTime(0)
            {
            }

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & flag;
                if (CodecID == VideoCodec::FLV_CODECID_H264) {
                    ar & AVCPacketType;
                    ar & CompositionTime;
                } else {
                    AVCPacketType = 1;
                    CompositionTime = 0;
                }
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

        struct FlvTagHeader
        {
            union {
                struct {
#ifdef   BOOST_BIG_ENDIAN
                    boost::uint8_t  Reserved : 2;
                    boost::uint8_t  Filter : 1;
                    boost::uint8_t  Type : 5;
#else 
                    boost::uint8_t  Type : 5;
                    boost::uint8_t  Filter : 1;
                    boost::uint8_t  Reserved : 2;
#endif
                };
                boost::uint8_t flag;
            };
            framework::system::UInt24 DataSize;
            framework::system::UInt24 Timestamp;
            boost::uint8_t TimestampExtended;
            framework::system::UInt24 StreamID;

            FlvTagHeader()
                : flag(0) // Reserved = 0, Filter = 0, Type = 0
                , TimestampExtended(0)
            {
            }

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & flag;
                ar & DataSize;
                if (ar && DataSize == 0) {
                    ar.fail();
                }
                TimestampExtended = Timestamp >> 24;
                ar & Timestamp;
                ar & TimestampExtended;
                Timestamp = Timestamp | (TimestampExtended << 24);
                ar & StreamID;
            }
        };

        struct FlvTag
            : FlvTagHeader
        {
            FlvAudioTagHeader AudioHeader;
            FlvVideoTagHeader VideoHeader;
            FlvDataTag DataTag;
            boost::uint32_t PreTagSize;

            boost::uint64_t data_offset;
            bool is_sample;
            bool is_sync;
            boost::uint32_t cts_delta;

            FlvTag()
                : PreTagSize(0)
                , data_offset(0)
                , is_sample(false)
                , is_sync(false)
                , cts_delta(0)
            {
            }

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                boost::uint64_t off = ar.tellg();

                FlvTagHeader::serialize(ar);

                data_offset = ar.tellg();
                if (Type == TagType::FLV_TAG_TYPE_AUDIO) {
                    ar & AudioHeader;
                    is_sample = AudioHeader.AACPacketType == 1;
                    is_sync = true;
                    cts_delta = 0;
                } else if (Type == TagType::FLV_TAG_TYPE_VIDEO) {
                    ar & VideoHeader;
                    is_sample = VideoHeader.AVCPacketType == 1;
                    is_sync = VideoHeader.FrameType == FrameType::FLV_FRAME_KEY;
                    cts_delta = VideoHeader.CompositionTime;
                } else if (Type == TagType::FLV_TAG_TYPE_META) {
                    ar & DataTag;
                    is_sample = false;
                    is_sync = false;
                    cts_delta = 0;
                } else {
                    is_sample = false;
                    is_sync = false;
                    cts_delta = 0;
                }
                if (ar && (DataSize + data_offset < (boost::uint64_t)ar.tellg())) {
                    ar.fail();
                } else {
                    DataSize = DataSize + data_offset - ar.tellg();
                    data_offset = ar.tellg();
                }
                ar.seekg(DataSize, std::ios_base::cur);
                ar & PreTagSize;
                if (ar && (off + PreTagSize + 4 != (boost::uint64_t)ar.tellg())) {
                    ar.fail();
                }
            }
        };

    }
}

#endif // _PPBOX_DEMUX_FLV_FLV_TAG_TYPE_H_

// FlvDataType.h

#ifndef _PPBOX_DEMUX_FLV_FLV_DATA_TYPE_H_
#define _PPBOX_DEMUX_FLV_FLV_DATA_TYPE_H_

#include "ppbox/demux/flv/FlvFormat.h"

namespace ppbox
{
    namespace demux
    {

        struct FlvDataString
        {
            boost::uint16_t StringLength;
            std::string StringData;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & StringLength;
                util::serialization::serialize_collection(ar, StringData, StringLength);
            }
        };

        struct FlvDataDate
        {
            double DateTime;
            boost::int16_t LocalDateTimeOffset;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & DateTime;
                ar & LocalDateTimeOffset;
            }
        };

        struct FlvDataLongString
        {
            boost::uint32_t StringLength;
            std::string StringData;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & StringLength;
                util::serialization::serialize_collection(ar, StringData, StringLength);
            }
        };

        struct FlvDataObjectProperty;

        static inline bool FLV_Property_End(
            FlvDataObjectProperty const & Property);

        struct FlvDataObject
        {
            std::vector<FlvDataObjectProperty> ObjectProperties;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                FlvDataObjectProperty Property;
                while (ar & Property) {
                    ObjectProperties.push_back(Property);
                    if (FLV_Property_End(Property))
                        break;
                }
            }
        };

        struct FlvDataECMAArray
        {
            boost::uint32_t ECMAArrayLength;
            std::vector<FlvDataObjectProperty> Variables;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & ECMAArrayLength;
                FlvDataObjectProperty Property;
                while (ar & Property) {
                    Variables.push_back(Property);
                    if (FLV_Property_End(Property))
                        break;
                }
            }
        };

        struct FlvDataValue;

        struct FlvDataStrictArray
        {
            boost::uint32_t StrictArrayLength;
            std::vector<FlvDataValue> StrictArrayValue;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & StrictArrayLength;
                util::serialization::serialize_collection(ar, StrictArrayValue, StrictArrayLength);
            }
        };

        struct FlvDataValue
        {
            boost::uint8_t Type;
            union {
                double Double;
                boost::uint8_t Bool;
                boost::uint16_t MovieClip;
                boost::uint16_t Null;
                boost::uint16_t Undefined;
                boost::uint16_t Reference;
                boost::uint16_t ObjectEndMarker;
                FlvDataDate Date;
            };
            FlvDataString String;
            FlvDataLongString LongString;
            FlvDataObject Object;
            FlvDataECMAArray ECMAArray;
            FlvDataStrictArray StrictArray;
            template <typename Archive>

            void serialize(
                Archive & ar)
            {
                ar & Type;
                switch (Type) {
                    case AMFDataType::AMF_DATA_TYPE_NUMBER:
                        ar & Double;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_BOOL:
                        ar & Bool;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_STRING:
                        ar & String;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_MOVIECLIP:
                        ar.fail();
                        break;
                    case AMFDataType::AMF_DATA_TYPE_NULL:
                        break;
                    case AMFDataType::AMF_DATA_TYPE_UNDEFINED:
                        break;
                    case AMFDataType::AMF_DATA_TYPE_REFERENCE:
                        ar & Reference;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_MIXEDARRAY:
                        ar & ECMAArray;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_OBJECT_END:
                        break;
                    case AMFDataType::AMF_DATA_TYPE_ARRAY:
                        ar & StrictArray;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_DATE:
                        ar & Date;
                        break;
                    case AMFDataType::AMF_DATA_TYPE_LONG_STRING:
                        ar & LongString;
                        break;
                    default:
                        ar.fail();
                        break;
                }
            }
        };

        struct FlvDataObjectProperty
        {
            FlvDataString PropertyName;
            FlvDataValue PropertyData;

            template <typename Archive>
            void serialize(
                Archive & ar)
            {
                ar & PropertyName
                    & PropertyData;
            }
        };

        static inline bool FLV_Property_End(
            FlvDataObjectProperty const & Property)
        {
            return Property.PropertyData.Type == AMFDataType::AMF_DATA_TYPE_OBJECT_END;
        }

    }
}

#endif // _PPBOX_DEMUX_FLV_FLV_DATA_TYPE_H_

// VodInfo.h

#ifndef _PPBOX_DEMUX_VOD_INFO_H_
#define _PPBOX_DEMUX_VOD_INFO_H_

#include "ppbox/common/Serialize.h"
#include "ppbox/demux/base/SourceBase.h"

#include <util/serialization/stl/vector.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h>

#include <framework/network/NetName.h>

namespace ppbox
{
    namespace demux
    {

        struct VodSegment
        {
            boost::uint64_t head_length;
            boost::uint64_t file_length;
            boost::uint32_t duration;   // 分段时长（毫秒）
            std::string va_rid;
            boost::uint32_t duration_offset;    // 相对起始的时长起点，（毫秒）
            boost::uint64_t duration_offset_us; // 同上，（微秒）
            boost::uint64_t block_size;
            boost::uint32_t block_num;

            VodSegment()
                : head_length(0)
                , file_length(0)
                , duration(0)
                , duration_offset(0)
                , duration_offset_us(0)
                , block_size(0)
                , block_num(0)
            {
            }

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                float duration = (float)this->duration / 1000.0f;
                ar & util::serialization::make_nvp("headlength", head_length)
                    & util::serialization::make_nvp("filesize", file_length)
                    & util::serialization::make_nvp("duration", duration)
                    & util::serialization::make_nvp("varid", va_rid);
                this->duration = (boost::uint32_t)(duration * 1000.0f);
            }
        };

        struct VodSegmentNew
            : VodSegment
        {
            VodSegmentNew()
                : VodSegment()
            {
            }

            VodSegmentNew(
                VodSegment const & r)
                : VodSegment(r)
            {
                std::string rid;
                map_find(r.va_rid, "rid", rid, "&");
                map_find(r.va_rid, "blocksize", this->block_size, "&");
                map_find(r.va_rid, "blocknum", this->block_num, "&");

                this->va_rid = rid;
            }

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                float duration = (float)this->duration / 1000.0f;
                ar & util::serialization::make_nvp("h", head_length)
                    & util::serialization::make_nvp("f", file_length)
                    & util::serialization::make_nvp("d", duration)
                    & util::serialization::make_nvp("r", va_rid);
                this->duration = (boost::uint32_t)(duration * 1000.0f);
            }
        };

        struct VodVideo
        {
            std::string name;
            std::string type;
            boost::uint32_t bitrate;    // 平均码流率
            boost::uint64_t filesize;
            boost::uint32_t duration;   // 影片时长（微秒）
            boost::uint32_t width;
            boost::uint32_t height;

            VodVideo()
                : bitrate(0)
                , filesize(0)
                , duration(0)
                , width(0)
                , height(0)
            {
            }

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                float duration = (float)this->duration / 1000.0f;
                ar & SERIALIZATION_NVP(name)
                    & SERIALIZATION_NVP(type)
                    & SERIALIZATION_NVP(bitrate)
                    & SERIALIZATION_NVP(filesize)
                    & SERIALIZATION_NVP(duration)
                    & SERIALIZATION_NVP(width)
                    & SERIALIZATION_NVP(height);
                this->duration = (boost::uint32_t)(duration * 1000.0f);
            }
        };

        struct VodJumpInfo
        {
            VodJumpInfo()
                : server_host("", 80)
                , BWType(0)
                , block_size(0)
            {
            }

            framework::network::NetName user_host;
            framework::network::NetName server_host;
            util::serialization::UtcTime server_time;
            std::string id;
            int BWType;

            VodVideo video;

            boost::uint64_t block_size;

            VodSegmentNew firstseg;

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(user_host)
                    & SERIALIZATION_NVP(server_host)
                    & SERIALIZATION_NVP(server_time)
                    & SERIALIZATION_NVP(id)
                    & SERIALIZATION_NVP(BWType)
                    & SERIALIZATION_NVP(video)
                    & util::serialization::make_nvp("blocksize", block_size)
                    & SERIALIZATION_NVP(firstseg);

                boost::uint32_t duration_offset = 0;
                firstseg.duration_offset = duration_offset;
                firstseg.duration_offset_us = (boost::uint64_t)duration_offset * 1000;
                duration_offset += firstseg.duration;

                firstseg.block_size = block_size;

                if (block_size != 0)
                    firstseg.block_num = (boost::uint32_t)(firstseg.file_length / block_size + ((firstseg.file_length % block_size != 0) ? 1 : 0));
            }
        };

        struct VodJumpInfoNoDrag
            : VodJumpInfo
        {
            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(user_host)
                    & SERIALIZATION_NVP(server_host)
                    & SERIALIZATION_NVP(server_time)
                    & SERIALIZATION_NVP(id)
                    & SERIALIZATION_NVP(BWType);

                block_size = 0;
            }
        };

        struct VodSegmentsNew
            : public std::vector<VodSegmentNew>
        {
            boost::uint64_t blocksize;

            VodSegmentsNew()
                : blocksize(0)
            {
            }

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(blocksize)
                    & (std::vector<VodSegmentNew> &)(*this);
            }
        };

        struct VodDragInfo
        {
            VodVideo video;
            std::vector<VodSegment> segments;

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(video)
                    & util::serialization::make_nvp("segments", segments);

                boost::uint32_t duration_offset = 0;
                for (size_t i = 0; i < segments.size(); ++i) {
                    segments[i].duration_offset = duration_offset;
                    segments[i].duration_offset_us = (boost::uint64_t)duration_offset * 1000;
                    duration_offset += segments[i].duration;

                    //if (segments[i].block_size != 0) {
                    //    segments[i].block_num = (boost::uint32_t)(
                    //        segments[i].file_length / segments[i].block_size + ((segments[i].file_length % segments[i].block_size != 0) ? 1 : 0));
                    //}
                }
            }
        };

        struct VodDragInfoNew
        {
            VodVideo video;
            VodSegmentsNew segments;

            bool is_ready;
            boost::system::error_code ec;

            VodDragInfoNew()
                : is_ready(false)
            {
            }

            VodDragInfoNew & operator=(
                VodDragInfo const & r)
            {
                video = r.video;
                for (size_t i = 0; i < r.segments.size(); ++i) {
                    segments.push_back(r.segments[i]);
                }
                return *this;
            }

            template <
                typename Archive
            >
            void serialize(
                Archive & ar)
            {
                ar & SERIALIZATION_NVP(video)
                    & util::serialization::make_nvp("ss", segments);

                boost::uint32_t duration_offset = 0;
                for (size_t i = 0; i < segments.size(); ++i) {
                    segments[i].duration_offset = duration_offset;
                    segments[i].duration_offset_us = (boost::uint64_t)duration_offset * 1000;
                    duration_offset += segments[i].duration;

                    segments[i].block_size = segments.blocksize;

                    if (segments[i].block_size != 0) {
                        segments[i].block_num = (boost::uint32_t)(
                            segments[i].file_length / segments[i].block_size + ((segments[i].file_length % segments[i].block_size != 0) ? 1 : 0));
                    }
                }
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_INFO_H_

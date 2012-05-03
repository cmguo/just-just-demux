// DemuxerBase.h

#ifndef _PPBOX_DEMUX_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_DEMUXER_BASE_H_

#include "ppbox/demux/base/DemuxerType.h"

#include <util/smart_ptr/RefenceFromThis.h>

#include <boost/detail/endian.hpp>
#include <boost/asio/buffer.hpp>

namespace ppbox
{
    namespace demux
    {

#ifdef BOOST_BIG_ENDIAN
#define MAKE_FOURC_TYPE(c1, c2, c3, c4) \
    ((((boost::uint32_t)c1) << 24) | \
    (((boost::uint32_t)c2) << 16) | \
    (((boost::uint32_t)c3) << 8) | \
    (((boost::uint32_t)c4)))
#else
#define MAKE_FOURC_TYPE(c1, c2, c3, c4) \
    ((((boost::uint32_t)c1)) | \
    (((boost::uint32_t)c2) << 8) | \
    (((boost::uint32_t)c3)) << 16 | \
    (((boost::uint32_t)c4) << 24))
#endif

        static const boost::uint32_t MEDIA_TYPE_VIDE = MAKE_FOURC_TYPE('V', 'I', 'D', 'E');
        static const boost::uint32_t MEDIA_TYPE_AUDI = MAKE_FOURC_TYPE('A', 'U', 'D', 'I');
        static const boost::uint32_t MEDIA_TYPE_NONE = 0;

        static const boost::uint32_t VIDEO_TYPE_AVC1 = MAKE_FOURC_TYPE('A', 'V', 'C', '1');
        static const boost::uint32_t VIDEO_TYPE_MP4V = MAKE_FOURC_TYPE('M', 'P', '4', 'V');
        static const boost::uint32_t VIDEO_TYPE_NONE = 0;

        static const boost::uint32_t AUDIO_TYPE_MP4A = MAKE_FOURC_TYPE('M', 'P', '4', 'A');
        static const boost::uint32_t AUDIO_TYPE_MP1A = MAKE_FOURC_TYPE('M', 'P', '1', 'A');
        static const boost::uint32_t AUDIO_TYPE_WMA2 = MAKE_FOURC_TYPE('W', 'M', 'A', '2');
        static const boost::uint32_t AUDIO_TYPE_NONE = 0;

        struct VideoInfo
        {
            boost::uint32_t width;
            boost::uint32_t height;
            boost::uint32_t frame_rate;
        };

        struct AudioInfo
        {
            boost::uint32_t channel_count;
            boost::uint32_t sample_size;
            boost::uint32_t sample_rate;
        };

        struct MediaInfoBase
        {
            MediaInfoBase()
                : type(MEDIA_TYPE_NONE)
                , sub_type(VIDEO_TYPE_NONE)
                , time_scale(0)
                , duration(0)
            {
            }

            union {
                boost::uint32_t type;
                char type_char[4];
            };
            union {
                boost::uint32_t sub_type;
                char sub_type_char[4];
            };
            boost::uint32_t index;
            boost::uint32_t time_scale;
            boost::uint64_t duration;
        };

        struct MediaInfo
            : MediaInfoBase
        {
            enum FormatTypeEnum
            {
                video_avc_packet = 1, 
                video_avc_byte_stream = 2, 
                audio_microsoft_wave = 3, 
                audio_iso_mp4 = 4, 
            };

            boost::uint32_t format_type;             // 格式说明的类型
            union {
                VideoInfo video_format;
                AudioInfo audio_format;
            };
            std::vector<boost::uint8_t> format_data; // 格式说明的内容
            void * attachment; // 附件信息
        };

        struct FileBlock
        {
            FileBlock(boost::uint64_t o, boost::uint32_t s)
                : offset(o)
                , size(s)
            {
            }

            boost::uint64_t offset;
            boost::uint32_t size;
        };

        struct Sample
        {
            Sample()
            :flags(0)
            ,us_delta(boost::uint32_t(-1))
            {
            }
            enum FlagEnum
            {
                 sync = 1, 
                 discontinuity = 2,
            };

            boost::uint32_t itrack;
            boost::uint32_t idesc;
            boost::uint32_t flags;
            boost::uint32_t time;
            boost::uint64_t ustime;
            boost::uint64_t dts;
            boost::uint32_t us_delta; //pts-dts
            boost::uint32_t cts_delta;
            boost::uint32_t duration;
            boost::uint32_t size;
            std::vector<FileBlock> blocks;
            MediaInfo const * media_info;
            void * context;
            std::deque<boost::asio::const_buffer> data;
        };

        class DemuxerBase
        {

        public:
            typedef boost::function<void (
                boost::uint64_t,
                boost::uint64_t,
                boost::system::error_code const &)
            > open_response_type;

        public:
            DemuxerBase(
                std::basic_streambuf<boost::uint8_t> & buf)
            {
            }

            virtual ~DemuxerBase()
            {
            }

            //virtual DemuxerBase * clone(
            //    std::basic_streambuf<boost::uint8_t> & buf)
            //{
            //    return this;
            //}

        public:
            virtual boost::system::error_code open(
                boost::system::error_code & ec,
                open_response_type const & resp) = 0;

            virtual boost::system::error_code close(
                boost::system::error_code & ec) = 0;

            virtual bool is_open(
                boost::system::error_code & ec) = 0;

            virtual boost::uint32_t get_end_time(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec) = 0;

            virtual size_t get_media_count(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec) = 0;

            virtual boost::uint32_t get_duration(
                boost::system::error_code & ec) = 0;

            virtual boost::uint32_t get_cur_time(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t get_offset(
                boost::uint32_t & time, 
                boost::uint32_t & delta, // 要重复下载的数据量 
                boost::system::error_code & ec) = 0;

            virtual void set_stream(std::basic_streambuf<boost::uint8_t> & buf) = 0;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_BASE_H_

// Mp4DemuxerBase.h

#ifndef _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_

#include "ppbox/demux/base/DemuxerBase.h"

#include <framework/container/Array.h>
#include <framework/container/OrderedUnidirList.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/buffer.hpp>

class AP4_File;

namespace ppbox
{
    namespace demux
    {

        class SampleListItem;
        struct SampleOffsetLess;
        struct SampleTimeLess;
        class Track;

        class Mp4DemuxerBase
            : public DemuxerBase
        {
        public:
            typedef framework::container::Array<
                boost::asio::const_buffer const
            > buffer_t;

            typedef framework::container::OrderedUnidirList<
                SampleListItem, 
                framework::container::identity<SampleListItem>, 
                SampleOffsetLess
            > SampleOffsetList;

            typedef framework::container::OrderedUnidirList<
                SampleListItem, 
                framework::container::identity<SampleListItem>, 
                SampleTimeLess
            > SampleTimeList;

#ifdef PPBOX_DEMUX_MP4_NO_TIME_ORDER
            typedef SampleOffsetList SampleList;
#else
            typedef SampleTimeList SampleList;
#endif

        public:
            Mp4DemuxerBase(
                std::basic_streambuf<boost::uint8_t> & buf);

            Mp4DemuxerBase(
                Mp4DemuxerBase * from, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~Mp4DemuxerBase();
            
            Mp4DemuxerBase * clone(
                std::basic_streambuf<boost::uint8_t> & buf);

        public:
            boost::system::error_code open(
                boost::system::error_code & ec);

            bool is_open(
                boost::system::error_code & ec);

            bool head_valid() const
            {
                return file_ != NULL;
            }

            boost::uint64_t min_offset() const
            {
                return min_offset_;
            }

            boost::system::error_code parse_head(
                boost::system::error_code & ec);

            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            boost::system::error_code get_track_base_info(
                size_t index, 
                MediaInfoBase & info, 
                boost::system::error_code & ec);

            boost::uint32_t get_duration(
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::system::error_code put_back_sample(
                Sample const & sample, 
                boost::system::error_code & ec);

            boost::uint64_t seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::system::error_code rewind(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint64_t get_offset(
                boost::uint32_t & time, 
                boost::uint32_t & delta, 
                boost::system::error_code & ec);

        private:
            std::basic_istream<boost::uint8_t> is_;
            boost::uint32_t head_size_;
            boost::uint32_t open_step_;
            AP4_File * file_;
            std::vector<Track *> tracks_;
            boost::uint32_t bitrate_;
            SampleList * sample_list_;
            bool sample_put_back_;
            boost::uint64_t min_offset_;
            const_pointer copy_from_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_

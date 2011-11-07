// Mp4DemuxerBase.h

#ifndef _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_

#include "ppbox/demux/DemuxerBase.h"

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
        {
        public:
            typedef framework::container::Array<
                boost::asio::const_buffer const
            > buffer_t;

        public:
            Mp4DemuxerBase();

            ~Mp4DemuxerBase();

        public:
            size_t min_head_size(
                buffer_t const & head_buf);

            size_t head_size() const
            {
                return head_size_;
            }

            bool head_valid() const
            {
                return file_ != NULL;
            }

            boost::uint64_t min_offset() const
            {
                return min_offset_;
            }

            boost::system::error_code set_head(
                buffer_t const & head_buf, 
                boost::uint64_t total_size, 
                boost::system::error_code & ec);

            boost::system::error_code get_head(
                std::vector<boost::uint8_t> & head_buf, 
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

            boost::uint64_t seek_to(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::system::error_code rewind(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::uint64_t offset, 
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            void release(void);

        private:
            boost::uint32_t head_size_;
            boost::uint32_t bitrate_;

        private:
            AP4_File * file_;
            std::vector<Track *> tracks_;

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
            SampleList * sample_list_;
            bool sample_put_back_;
            boost::uint64_t min_offset_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_MP4_MP4_DEMUXER_BASE_H_

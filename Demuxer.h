// Demuxer.h

#ifndef _PPBOX_DEMUX_DEMUXER_H_
#define _PPBOX_DEMUX_DEMUXER_H_

#include "ppbox/demux/DemuxerBase.h"
#include "ppbox/demux/source/BufferStatistic.h"
#include "ppbox/demux/DemuxerStatistic.h"

namespace framework { namespace timer { class Ticker; } }
namespace framework { namespace network { class NetName; } }

namespace ppbox
{
    namespace demux
    {

        struct MediaInfo;

        struct Sample;

        struct SegmentInfo
        {
            size_t index;
            boost::uint32_t duration;
            boost::uint32_t duration_offset;
            boost::uint64_t file_length;
            boost::uint64_t head_length;
            std::vector<boost::uint8_t> head_data;
        };

        class Demuxer
            : protected DemuxerStatistic
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            Demuxer(
                boost::asio::io_service & io_svc, 
                BufferStatistic const & buf_stat);

            virtual ~Demuxer();

        public:
            virtual boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec);

            virtual void async_open(
                std::string const & name, 
                open_response_type const & resp) = 0;

            virtual bool is_open(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code pause(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code resume(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code close(
                boost::system::error_code & ec) = 0;

            virtual size_t get_media_count(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec) = 0;

            virtual boost::uint32_t get_duration(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec) = 0;

            virtual boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf) = 0;

            virtual boost::uint32_t get_cur_time(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec) = 0;

            virtual size_t get_segment_count(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_segment_info(
                SegmentInfo & info, 
                bool need_head_data, 
                boost::system::error_code & ec);

            virtual boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

            virtual boost::system::error_code set_max_dl_speed(
                boost::uint32_t speed, // KBps
                boost::system::error_code & ec);

        public:
            boost::system::error_code get_sample_buffered(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::uint32_t get_buffer_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            void on_extern_error(
                boost::system::error_code const & ec);

        public:
            using DemuxerStatistic::demux_stat;
            using DemuxerStatistic::get_status_info;
            using DemuxerStatistic::open_total_time;
            using DemuxerStatistic::is_ready;

            BufferStatistic const & buffer_stat() const
            {
                return buf_stat_;
            }

            DemuxerStatistic const & stat() const
            {
                return *this;
            }

            void set_param(
                std::string const & str)
            {
                DemuxerStatistic::set_param(str);
            }

        protected:
            void tick_on();

        private:
            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;
            BufferStatistic const & buf_stat_;
            boost::system::error_code extern_error_;

        private:
            framework::timer::Ticker * ticker_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_H_

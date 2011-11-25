// LiveDemuxer.h

#ifndef _PPBOX_DEMUX_LIVE_LIVE_DEMUXER_H_
#define _PPBOX_DEMUX_LIVE_LIVE_DEMUXER_H_

#include "ppbox/demux/pptv/PptvDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        class LiveSegments;

        class LiveDemuxerImpl;

        class PptvJump;
        struct LiveJumpInfo;

        class LiveDemuxer
            : public PptvDemuxer
        {
        public:
            LiveDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size);

            ~LiveDemuxer();

        public:
            void async_open(
                std::string const & name, 
                open_response_type const & resp);

           /* bool is_open(
                boost::system::error_code & ec);*/

            boost::system::error_code cancel(
                boost::system::error_code & ec);

            boost::system::error_code pause(
                boost::system::error_code & ec);

            boost::system::error_code resume(
                boost::system::error_code & ec);

            boost::system::error_code close(
                boost::system::error_code & ec);

            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            boost::uint32_t get_duration(
                boost::system::error_code & ec);

            std::pair<boost::uint32_t, boost::uint32_t> get_dimension(
                boost::system::error_code & ec);

            boost::system::error_code get_avc1(
                std::vector<unsigned char> & buf, 
                boost::system::error_code & ec);

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec);

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec);

            boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

        private:
            friend class LiveSegments;

            void seg_beg(
                size_t segment);

            void seg_end(
                size_t segment);

        private:
            void handle_async_open(
                boost::system::error_code const & ec);

            void response(
                boost::system::error_code const & ec);

            void set_info_by_jump(
                LiveJumpInfo & jump_info);

            static void parse_jump(
                LiveJumpInfo & jump_info,
                boost::asio::streambuf & buf, 
                boost::system::error_code & ec);

            void open_logs_end(
                ppbox::common::HttpStatistics const & http_stat, 
                int index, 
                boost::system::error_code const & ec);

        private:
            struct StepType
            {
                enum Enum
                {
                    not_open, 
                    opening, 
                    jump, 
                    head_normal, 
                    finish, 
                };
            };

        private:
            LiveSegments * segments_;
            //LiveDemuxerImpl * demuxer_;
            PptvJump * jump_;

            open_response_type resp_;

            std::string name_;

            StepType::Enum open_step_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE_LIVE_DEMUXER_H_

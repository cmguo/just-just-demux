// Live2Demuxer.h

#ifndef _PPBOX_DEMUX_LIVE2_DEMUXER_H_
#define _PPBOX_DEMUX_LIVE2_DEMUXER_H_

#include "ppbox/demux/pptv/PptvDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        class Live2Segments;

        class PptvJump;
        struct Live2JumpInfo;

        class Live2Demuxer
            : public PptvDemuxer
        {
        public:
            Live2Demuxer(
                boost::asio::io_service & io_svc, 
                boost::uint16_t live_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size);

            ~Live2Demuxer();

        public:
            void async_open(
                std::string const & name, 
                open_response_type const & resp);

            //bool is_open(
            //    boost::system::error_code & ec);

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

            boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

        private:
            friend class Live2Segments;

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
                Live2JumpInfo & jump_info);

            static void parse_jump(
                Live2JumpInfo & jump_info,
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
            Live2Segments * segments_;
            PptvJump * jump_;

            open_response_type resp_;

            std::string name_;

            StepType::Enum open_step_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_LIVE2_DEMUXER_H_

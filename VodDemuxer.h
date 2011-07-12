// VodDemuxer.h

#ifndef _PPBOX_DEMUX_VOD_DEMUXER_H_
#define _PPBOX_DEMUX_VOD_DEMUXER_H_

#include "ppbox/demux/Demuxer.h"

#include <boost/asio/streambuf.hpp>

namespace util
{
    namespace protocol
    {
        class HttpClient;
    }
}

namespace framework
{
    namespace network
    {
        class NetName;
    }
}

namespace ppbox
{
    namespace demux
    {

        struct VodJumpInfo;
        struct VodJumpInfoNoDrag;
        struct VodDragInfoNew;
        struct VodVideo;

        class VodSegmentDemuxer;
        class VodSegments;
        class PptvJump;
        class PptvDrag;

        class VodDemuxer
            : public Demuxer
        {
        public:
            VodDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint16_t ppap_port, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size);

            ~VodDemuxer();

        public:
            void async_open(
                std::string const & name, 
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

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

            boost::system::error_code seek(
                boost::uint32_t time, 
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

            size_t get_segment_count(
                boost::system::error_code & ec);

            boost::system::error_code get_segment_info(
                SegmentInfo & info, 
                bool need_head_data, 
                boost::system::error_code & ec);

            boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

        private:
            friend class VodSegments;

            void seg_beg(
                size_t segment);

            void seg_end(
                size_t segment);

        private:
             boost::system::error_code get_jump(
                 framework::string::Url const & url, 
                 VodJumpInfo & jump_info, 
                 boost::system::error_code & ec);

             static void parse_drag(
                 VodDragInfoNew & drag_info, 
                 boost::asio::streambuf & buf, 
                 boost::system::error_code const & ec);

             static void parse_jump(
                 VodJumpInfoNoDrag & jump_info, 
                 boost::asio::streambuf & buf, 
                 boost::system::error_code & ec);

             void process_drag(
                 VodDragInfoNew & drag_info, 
                 boost::system::error_code & ec);

             boost::system::error_code check_pending_seek(
                 boost::system::error_code & ec);

             void release_head_buffer(
                 boost::uint32_t before,
                 boost::uint32_t cur,
                 boost::uint32_t after);

             void handle_async_open(
                 boost::system::error_code const & ec);

             void set_info_by_jump(
                 VodJumpInfoNoDrag & jump_info);

             void set_info_by_video(
                 VodVideo & video);

             void response(
                 boost::system::error_code const & ec);

             static void handle_drag(
                 boost::shared_ptr<VodDragInfoNew> const & drag_info, 
                 boost::system::error_code const & ec, 
                 boost::asio::streambuf & buf);

             void open_logs_end(
                 ppbox::common::HttpStatistics const & http_stat, 
                 int index, 
                 boost::system::error_code const & ec);

             bool is_open(
                 bool need_check_seek, 
                 boost::system::error_code & ec);

        private:
            struct StepType
            {
                enum Enum
                {
                    not_open, 
                    opening, 
                    jump, 
                    head_normal, 
                    drag_abnormal, 
                    drag_normal, 
                    head_abnormal, 
                    finish, 
                    finish2
                };
            };

        private:
            VodSegments * buffer_;
            VodVideo * video_;
            std::vector<VodSegmentDemuxer *> segments_;
            PptvJump * jump_;
            boost::shared_ptr<PptvDrag> drag_;
            boost::shared_ptr<VodDragInfoNew> drag_info_;
            boost::uint32_t seek_time_;

            std::vector<MediaInfo> media_info_;
            boost::uint32_t keep_count_;

            open_response_type resp_;

            std::string name_;

            StepType::Enum open_step_;

            boost::system::error_code pending_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_DEMUXER_H_

// VodSource.h

#ifndef _PPBOX_DEMUX_VOD_SOURCE_H_
#define _PPBOX_DEMUX_VOD_SOURCE_H_

#include "ppbox/demux/source/HttpSource.h"
#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/vod/VodInfo.h"

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

        class VodSource
            : public HttpSource
        {
        public:
            VodSource(
                boost::asio::io_service & io_svc);

            virtual ~VodSource();

        public:
            virtual void async_open(SourceBase::response_type const &resp);

            virtual bool is_open();

        public:

            boost::system::error_code segment_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                boost::system::error_code & ec);

            void segment_async_open(
                size_t segment, 
                boost::uint64_t beg, 
                boost::uint64_t end, 
                SourceBase::response_type const & resp) ;

            virtual DemuxerType::Enum demuxer_type() const;

            virtual void set_url(std::string const &url);

            virtual boost::system::error_code reset(size_t& segment);

            virtual boost::uint32_t get_duration();

            virtual void update_segment_duration(size_t segment,boost::uint32_t time);
            virtual void update_segment_file_size(size_t segment,boost::uint64_t fsize);
            virtual void update_segment_head_size(size_t segment,boost::uint64_t hsize);
        private:
            virtual size_t segment_count() const;
            virtual boost::uint64_t segment_size(size_t segment);
            virtual boost::uint64_t segment_time(size_t segment);

        private:
            virtual boost::system::error_code get_request(
                size_t segment, 
                boost::uint64_t & beg, 
                boost::uint64_t & end, 
                framework::network::NetName & addr, 
                util::protocol::HttpRequest & request, 
                boost::system::error_code & ec);

            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            framework::string::Url get_jump_url() const;
            framework::string::Url get_drag_url() const;

            void parse_drag(
                VodDragInfoNew & drag_info, 
                boost::asio::streambuf & buf, 
                boost::system::error_code const & ec);

            void parse_jump(
                VodJumpInfoNoDrag & jump_info, 
                boost::asio::streambuf & buf, 
                boost::system::error_code & ec);


            void set_info_by_jump(
                VodJumpInfoNoDrag & jump_info);

            void set_info_by_video(
                VodVideo & video);

            void set_info_by_drag(
                VodDragInfoNew & drag_info);

            std::string get_key() const;

            void update_segment(size_t segment);

        private:
            struct StepType
            {
                enum Enum
                {
                    not_open, 
                    opening, 
                    jump,
                    status,
                    drag, 
                    finish, 
                    finish2
                };
            };
            //Demux
        private:
            VodVideo * video_;
            PptvJump * jump_;
            boost::shared_ptr<PptvDrag> drag_;
            SourceBase::response_type resp_;

            StepType::Enum open_step_;
            //vod segment
        private:
            boost::uint16_t vod_port_;
            std::string name_;
            framework::network::NetName server_host_;
            framework::network::NetName proxy_addr_;
            boost::int32_t bwtype_;
            bool first_seg_;
            bool know_seg_count_;

            Time local_time_;
            time_t server_time_;
            Url url_;
            std::vector<VodSegmentNew> segments_;
            boost::uint32_t max_dl_speed_;
            VodDemuxer * vod_demuxer_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SOURCE__H_

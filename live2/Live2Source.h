// VodSource.h

#ifndef _PPBOX_DEMUX_LIVE2_SOURCE_H_
#define _PPBOX_DEMUX_LIVE2_SOURCE_H_

#include "ppbox/demux/source/HttpSource.h"

#include <ppbox/common/Serialize.h>

#include <util/protocol/pptv/Base64.h>
#include <util/serialization/NVPair.h>
#include <util/serialization/stl/vector.h>

#include <framework/timer/ClockTime.h>
namespace ppbox
{
    namespace demux
    {
        class PptvJump;

        struct Live2JumpInfo
        {
            framework::network::NetName server_host;
            std::vector<framework::network::NetName> server_hosts;
            util::serialization::UtcTime server_time;
            boost::uint16_t delay_play_time;
            std::string channelGUID;
            std::string server_limit;
            std::vector<std::string> server_limits;

            template <
                typename Archive
            >
            void serialize(
            Archive & ar)
            {
                ar & SERIALIZATION_NVP(server_host)
                    & SERIALIZATION_NVP(server_time)
                    & SERIALIZATION_NVP(delay_play_time)
                    & SERIALIZATION_NVP(server_limit)
                    & SERIALIZATION_NVP(channelGUID)
                    & SERIALIZATION_NVP(server_hosts)
                    & SERIALIZATION_NVP(server_limits);
            }
        };

        class Live2Source
            : public HttpSource
        {
        public:
            Live2Source(
                boost::asio::io_service & io_svc);

            virtual ~Live2Source();

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

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // ОўГо
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual bool next_segment(
                SegmentPositionEx & position);
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

            void parse_jump(
                Live2JumpInfo & jump_info, 
                boost::asio::streambuf & buf, 
                boost::system::error_code & ec);


            void set_info_by_jump(
                Live2JumpInfo & jump_info);


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
                    finish, 
                };
            };
        private:
            std::string key_;
            std::string url_;
            std::string name_;
            std::string channel_;
            std::string stream_id_;

            Live2JumpInfo jump_info_;
            Time local_time_;
            time_t server_time_;
            time_t file_time_;
            time_t old_file_time_;
            boost::uint32_t seek_time_;
            boost::int32_t bwtype_;
            boost::int32_t live_port_;
            int index_;
            std::vector<std::string> rid_;
            std::vector<boost::uint32_t> rate_;

            std::deque<boost::uint64_t> segments_;

            boost::uint16_t interval_;
            boost::uint32_t seq_;
        private:
            PptvJump * jump_;
            SourceBase::response_type resp_;
            StepType::Enum open_step_;
            boost::uint32_t time_;

        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_VOD_SOURCE__H_

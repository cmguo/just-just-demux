// PptvPptvDemuxerStatistic.h

#ifndef _PPBOX_DEMUX_PPTV_DEMUXER_STATISTIC_H_
#define _PPBOX_DEMUX_PPTV_DEMUXER_STATISTIC_H_

#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/pptv/PptvDemuxerType.h"

#include <ppbox/common/HttpStatistics.h>

#include <framework/network/Statistics.h>

namespace ppbox
{
    namespace demux
    {

        struct DemuxData
        {
            DemuxData()
                : bitrate(0)
                , segment(0)
            {
                name[0] = '\0';
                rid[0] = '\0';
                server_host[0] = '\0';
                param[0] = '\0';
            }

            void set_name(
                std::string const & str)
            {
                strncpy(name, str.c_str(), sizeof(name));
            }

            void set_rid(
                std::string const & str)
            {
                strncpy(rid, str.c_str(), sizeof(rid));
            }

            void set_server_host(
                std::string const & str)
            {
                strncpy(server_host, str.c_str(), sizeof(server_host));
            }

            void set_param(
                std::string const & str)
            {
                strncpy(param, str.c_str(), sizeof(param));
            }

            char name[1024];
            boost::uint32_t bitrate;
            size_t segment;
            char rid[36];
            char server_host[64];
            char param[1024*2];
        };

        class PptvDemuxerStatistic
            : public BufferDemuxer
        {
        public:
            PptvDemuxerStatistic();

        public:
            void set_play_type(
                PptvDemuxerType::Enum play_type)
            {
                play_type_ = play_type;
            }

            DemuxData & demux_data()
            {
                return demux_data_;
            }

            DemuxData const & demux_data() const
            {
                return demux_data_;
            }

            void set_param(
                std::string const & str)
            {
                demux_data_.set_param(str);
            }

        public:
            bool is_playing_state() const
            {
                return state_ > opened;
            }

            PptvDemuxerType::Enum const & demuxer_type() const
            {
                return play_type_;
            }

            std::vector<ppbox::common::HttpStatistics> const & open_logs() const
            {
                return open_logs_;
            }

            boost::uint32_t open_total_time() const
            {
                return open_total_time_;
            }

            bool is_ready()
            {
                return is_ready_;
            }

        protected:
            std::vector<ppbox::common::HttpStatistics> open_logs_; // ²»³¬¹ý3¸ö
            DemuxData demux_data_;

            PptvDemuxerType::Enum play_type_;
            boost::uint32_t open_total_time_;
            bool is_ready_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_STATISTIC_H_

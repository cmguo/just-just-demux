// DemuxerStatistic.h

#ifndef _PPBOX_DEMUX_DEMUXER_STATISTIC_H_
#define _PPBOX_DEMUX_DEMUXER_STATISTIC_H_

#include "ppbox/demux/source/BufferStatistic.h"
#include "ppbox/demux/DemuxerType.h"

#include <ppbox/common/HttpStatistics.h>

#include <framework/network/Statistics.h>

namespace ppbox
{
    namespace demux
    {

        typedef boost::uint32_t time_type;

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

        struct BlockType
        {
            enum Enum
            {
                init = 0x0100, 
                play = 0x0200, 
                seek = 0x0300, 
            };
        };

        struct StatusInfo
        {
            StatusInfo()
                : status_type(0)
                , play_position((boost::uint32_t)-1)
                , elapse((boost::uint32_t)-1)
            {
            }

            StatusInfo(
                boost::uint16_t type, 
                boost::uint32_t now_playing)
                : status_type(type)
                , play_position(now_playing)
                , elapse(0)
            {
            }

            boost::uint16_t status_type;
            boost::uint32_t play_position;
            boost::uint32_t elapse;
        };

        class DemuxerStatistic
            : public framework::network::TimeStatistics
        {
        public:
            enum StatusEnum
            {
                stopped, 
                opening, 
                opened, 
                paused,
                playing, 
                buffering
            };

        public:
            DemuxerStatistic();

        public:
            void set_play_type(
                DemuxerType::Enum play_type)
            {
                play_type_ = play_type;
            }

            void open_beg(
                std::string const & name);

            void open_end(
                boost::system::error_code const & ec);

            void pause();

            void resume();

            void close();

            void seek(
                boost::uint32_t seek_time, 
                boost::system::error_code const & ec);

            void play_on(
                boost::uint32_t sample_time);

            void block_on();

            void on_error(
                boost::system::error_code const & ec);

            void set_buf_time(
                boost::uint32_t const & buffer_time);

            StatusEnum state() const
            {
                return state_;
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

            boost::uint32_t get_buf_time() const
            {
                return buffer_time_;
            }

            boost::uint32_t get_playing_time() const
            {
                return play_position_;
            }

            DemuxerType::Enum const & get_play_type() const
            {
                return play_type_;
            }

            boost::system::error_code last_error() const
            {
                return last_error_;
            }

            DemuxData const & demux_stat() const
            {
                return demux_data_;
            }

            std::vector<ppbox::common::HttpStatistics> const & open_logs() const
            {
                return open_logs_;
            }

            std::vector<StatusInfo> const & get_status_info() const
            {
                return status_infos_;
            }

            boost::uint32_t open_total_time() const
            {
                return open_total_time_;
            }

            bool is_ready()
            {
                return is_ready_;
            }

        private:
            void change_status(
                StatusEnum new_state);

        protected:
            std::vector<ppbox::common::HttpStatistics> open_logs_; // 不超过3个
            DemuxData demux_data_;

            DemuxerType::Enum play_type_;
            StatusEnum state_;
            boost::uint32_t buffer_time_;
            bool need_seek_time_;
            boost::uint32_t seek_position_;        // 拖动后的位置（时间毫秒）
            boost::uint32_t play_position_;        // 播放的位置（时间毫秒）
            BlockType::Enum block_type_;
            boost::uint32_t last_time_;
            std::vector<StatusInfo> status_infos_;
            boost::system::error_code last_error_;

            boost::uint32_t open_total_time_;

            bool is_ready_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_STATISTIC_H_

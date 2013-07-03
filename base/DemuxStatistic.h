// DemuxStatistic.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

#include "ppbox/demux/base/DemuxBase.h"
#include "ppbox/demux/base/DemuxEvent.h"

#include <framework/timer/TimeCounter.h>

#include <util/event/Observable.h>

namespace framework { namespace timer { class Ticker; } }

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;

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
                , play_position((boost::uint64_t)-1)
                , elapse((boost::uint64_t)-1)
            {
            }

            StatusInfo(
                boost::uint16_t type, 
                boost::uint64_t now_playing)
                : status_type(type)
                , play_position(now_playing)
                , elapse(0)
            {
            }

            boost::uint16_t status_type;
            boost::uint64_t play_position;
            boost::uint64_t elapse;
        };

        class DemuxStatistic
            : public StreamStatus
            , public util::event::Observable
            , public framework::timer::TimeCounter
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
            DemuxStatisticEvent status_changed;

            // 定期发出的缓存状态
            DemuxStatisticEvent buffer_update;

        protected:
            DemuxStatistic(
                DemuxerBase & demuxer);

        protected:
            void open_beg();

            void open_end();

            void pause();

            void resume();

            void play_on(
                boost::uint64_t sample_time);

            void block_on();

            void seek(
                bool ok, 
                boost::uint64_t seek_time);

            void last_error(
                boost::system::error_code const & ec);

            void close();

        public:
            StatusEnum state() const
            {
                return state_;
            }

            boost::uint64_t buf_time() const
            {
                return time_range.buf - time_range.pos;
            }

            boost::uint64_t play_position() const
            {
                return play_position_;
            }

            boost::uint64_t seek_position() const
            {
                return seek_position_;
            }

            std::vector<StatusInfo> const & status_info() const
            {
                return status_infos_;
            }

            boost::system::error_code last_error() const
            {
                return last_error_;
            }

        private:
            void change_status(
                boost::uint16_t new_state);

            void update_stat();

        private:
            DemuxerBase & demuxer_;
            framework::timer::Ticker * ticker_;

        protected:
            StatusEnum state_;
            bool need_seek_time_;
            boost::uint64_t seek_position_;        // 拖动后的位置（时间毫秒）
            boost::uint64_t play_position_;        // 播放的位置（时间毫秒）
            BlockType::Enum block_type_;
            boost::uint64_t last_time_;
            std::vector<StatusInfo> status_infos_;
            boost::system::error_code last_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

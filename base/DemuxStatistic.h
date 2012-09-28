// DemuxStatistic.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

#include <framework/timer/TimeCounter.h>

#include <util/event/Observable.h>

namespace ppbox
{
    namespace demux
    {

        typedef boost::uint32_t time_type;

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
            : public util::event::Observable
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

        protected:
            DemuxStatistic();

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

            void buf_time(
                boost::uint64_t buffer_time);

        public:
            StatusEnum state() const
            {
                return state_;
            }

            boost::uint64_t buf_time() const
            {
                return buffer_time_;
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

        protected:
            StatusEnum state_;
            boost::uint64_t buffer_time_;
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

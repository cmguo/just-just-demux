// DemuxStatistic.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

#include "ppbox/demux/base/DemuxBase.h"
#include "ppbox/demux/base/DemuxEvent.h"

#include <ppbox/data/base/StreamStatus.h>

#include <framework/timer/TimeCounter.h>

#include <util/event/Observable.h>

namespace framework { namespace timer { class Ticker; } }

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;

        class DemuxStatistic
            : public StreamStatus
            , public util::event::Observable
            , public framework::timer::TimeCounter
        {
        public:
            enum StatusEnum
            {
                closed, 
                media_opening, 
                demux_opening, 
                opened, 
                paused,
                playing, 
                seeking, 
            };

            struct StatusInfo
            {
                StatusInfo()
                    : status(closed)
                    , blocked(false)
                    , play_position((boost::uint64_t)-1)
                    , elapse((boost::uint64_t)-1)
                {
                }

                StatusInfo(
                    StatusEnum status, 
                    bool blocked, 
                    boost::uint64_t play_position)
                    : status(status)
                    , blocked(blocked)
                    , play_position(play_position)
                    , elapse(0)
                {
                }

                StatusEnum status;
                bool blocked;
                boost::uint64_t play_position;
                boost::uint64_t elapse;
            };

        public:
            DemuxStatisticEvent status_changed;

            // 定期发出的缓存状态
            DemuxStatisticEvent buffer_update;

        protected:
            DemuxStatistic(
                DemuxerBase & demuxer);

        protected:
            void open_beg_media();

            void open_beg_demux();

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
            StatusEnum status() const
            {
                return StatusEnum(status_ex_ & 0x007f);
            }

            bool blocked() const
            {
                return (status_ex_ & 0x80) != 0;
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
                StatusEnum status, 
                bool blocked = false);

            void update_stat();

        private:
            DemuxerBase & demuxer_;
            framework::timer::Ticker * ticker_;

        protected:
            boost::uint16_t status_ex_;
            boost::uint32_t status_history_;
            bool need_seek_time_;
            boost::uint64_t seek_position_;        // 拖动后的位置（时间毫秒）
            boost::uint64_t play_position_;        // 播放的位置（时间毫秒）
            boost::uint64_t last_time_;
            std::vector<StatusInfo> status_infos_;
            boost::system::error_code last_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STATISTIC_H_

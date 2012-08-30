// DemuxerStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerStatistic.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/logger/Section.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("DemuxerStatistic", 0);

namespace ppbox
{
    namespace demux
    {

        DemuxerStatistic::DemuxerStatistic()
            : state_(stopped)
            , buffer_time_(0)
            , need_seek_time_(false)
            , seek_position_(0)
            , play_position_(0)
            , block_type_(BlockType::init)
            , last_time_(0)
        {
            status_infos_.push_back(StatusInfo(stopped, play_position_));
        }

        void DemuxerStatistic::buf_time(
            boost::uint32_t const & buffer_time)
        {
            if (state_ == buffering) {
                LOG_DEBUG("[buf_time] buf_time: " << buffer_time << " ms");
            }
            buffer_time_ = buffer_time;
        }

        static char const * type_str[] = {
            "stopped", 
            "opening", 
            "opened", 
            "paused",
            "playing", 
            "buffering"
        };

        void DemuxerStatistic::change_status(
            boost::uint16_t new_state)
        {
            boost::uint32_t now_time = elapse();
            boost::uint32_t elapse = now_time - last_time_;
            last_time_ = now_time;
            // 两个Buffering状态，原因相同，并且中间间隔一个playing状态，并且持续play时间（sample时间）小于3秒
            // 那么合并Buffering状态，并且如果接下来是playing状态，也合并
            if (state_ == buffering 
                && status_infos_[status_infos_.size() - 1].status_type == status_infos_[status_infos_.size() - 3].status_type 
                && status_infos_[status_infos_.size() - 2].status_type == playing 
                && (play_position_ - status_infos_[status_infos_.size() - 2].play_position < 3000)) {
                    // 合并Buffering状态，此后剩下一个Buffering状态加一个playing状态
                    status_infos_[status_infos_.size() - 3].elapse += elapse;
                    status_infos_.pop_back();
                    // 如果接下来是playing状态，也合并
                    if (new_state == playing) {
                        last_time_ -= status_infos_.back().elapse;
                        status_infos_.pop_back();
                    }
            } else {
                // 合并时间
                status_infos_.back().elapse = elapse;
            }
            boost::uint16_t type = new_state;
            if (new_state == buffering) {
                type |= block_type_;
            }
            StatusInfo info(type, play_position_);
            status_infos_.push_back(info);
            state_ = (StatusEnum)new_state;
        }

        void DemuxerStatistic::open_beg()
        {
            assert(state_ == stopped);
            change_status(opening);
        }

        void DemuxerStatistic::open_end()
        {
            assert(state_ <= opening);
            change_status(opened);
            play_position_ = 0;
        }

        void DemuxerStatistic::pause()
        {
            if (state_ == paused)
                return;

            change_status(paused);
        }

        void DemuxerStatistic::resume()
        {
            if (state_ != paused)
                return;

            change_status(playing);
        }

        void DemuxerStatistic::play_on(
            boost::uint32_t sample_time)
        {
            play_position_ = sample_time;

            if (state_ == playing)
                return;

            if (need_seek_time_) {
                seek_position_ = play_position_;
                need_seek_time_ = false;
            }

            change_status(playing);

            block_type_ = BlockType::play;
        }

        void DemuxerStatistic::block_on()
        {
            if (state_ == buffering)
                return;
            change_status(buffering);
        }

        void DemuxerStatistic::seek(
            bool ok, 
            boost::uint32_t seek_time)
        {
            // 如果拖动的时间与当前播放的时间相同，则不处理
            if (play_position_ == seek_time)
                return;

            buffer_time_ = 0;
            block_type_ = BlockType::seek;
            if (ok) {
                change_status(BlockType::seek | playing);
                seek_position_ = seek_time;
                need_seek_time_ = false;
                play_on(seek_time);
            } else {
                change_status(buffering);
                seek_position_ = seek_time;
                need_seek_time_ = true;
                play_position_ = seek_time;
            }
        }

        void DemuxerStatistic::last_error(
            boost::system::error_code const & ec)
        {
            last_error_ = ec;
        }

        void DemuxerStatistic::close()
        {
            change_status(stopped);
            status_infos_.back().elapse = 0;
            block_type_ = BlockType::init;
        }

    }
}

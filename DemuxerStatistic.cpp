// DemuxerStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/DemuxerStatistic.h"

#include <framework/logger/Logger.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/logger/LoggerSection.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("DemuxerStatistic", 0);

namespace ppbox
{
    namespace demux
    {

        DemuxerStatistic::DemuxerStatistic()
            : play_type_(DemuxerType::none)
            , state_(stopped)
            , buffer_time_(0)
            , need_seek_time_(false)
            , seek_position_(0)
            , play_position_(0)
            , block_type_(BlockType::init)
            , last_time_(0)
            , open_total_time_(0)
            , is_ready_(true)
        {
            status_infos_.push_back(StatusInfo(stopped, play_position_));
        }

        void DemuxerStatistic::set_buf_time(
            boost::uint32_t const & buffer_time)
        {
            if (state_ == buffering) {
                LOG_S(Logger::kLevelDebug, "[set_buf_time] buf_time: " << buffer_time << " ms");
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
            StatusEnum new_state)
        {
            boost::uint32_t now_time = elapse();
            boost::uint32_t elapse = now_time - last_time_;
            LOG_S(Logger::kLevelDebug, "[change_status]: elapse=" << elapse
                << ", now_time=" << now_time << ", last_time_=" << last_time_ );
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
                    LOG_S(Logger::kLevelDebug, "[change_status]: merge buffering");
                    // 如果接下来是playing状态，也合并
                    if (new_state == playing) {
                        last_time_ -= status_infos_.back().elapse;
                        status_infos_.pop_back();

                        LOG_S(Logger::kLevelDebug, "[change_status]: after merge buffering is playing, last_time_=" << last_time_);
                    }
            } else {
                // 合并时间
                status_infos_.back().elapse = elapse;
            }
            boost::uint16_t type = new_state;
            if (new_state == buffering) {
                type |= block_type_;

                LOG_S(Logger::kLevelDebug, "[change_status]: buffering block_type=" << block_type_);
            }
            LOG_S(Logger::kLevelDebug, "[change_status]: new_state=" 
                << type_str[(int)new_state & 0xff] << ", play_position=" << play_position_);

            StatusInfo info(type, play_position_);
            status_infos_.push_back(info);
            state_ = new_state;
        }

        void DemuxerStatistic::open_beg(
            std::string const & name)
        {
            assert(state_ == stopped);

            demux_data_.set_name(name);

            change_status(opening);

            is_ready_ = false;
        }

        void DemuxerStatistic::open_end(
            boost::system::error_code const & ec)
        {
            assert(state_ <= opening);

            last_error_ = ec;

            change_status(opened);

            play_position_ = 0;

            open_total_time_ = elapse();
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
            boost::uint32_t seek_time, 
            boost::system::error_code const & ec)
        {
            // 如果拖动的时间与当前播放的时间相同，则不处理
            if (play_position_ == seek_time)
                return;

            need_seek_time_ = true;

            buffer_time_ = 0;
            block_type_ = BlockType::seek;

            if (ec) {
                change_status(buffering);
                seek_position_ = seek_time;
                play_position_ = seek_time;
            } else {
                change_status((StatusEnum)(BlockType::seek | playing));
                seek_position_ = seek_time;
                need_seek_time_ = false;
                play_on(seek_time);
                //block_type_ = BlockType::play;
                //play_position_ = seek_time;
                //change_status(playing);
            }
        }

        void DemuxerStatistic::close()
        {
            change_status(stopped);

            status_infos_.back().elapse = 0;

            block_type_ = BlockType::init;
        }

    }
}

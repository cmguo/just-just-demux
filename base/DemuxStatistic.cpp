// DemuxStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxStatistic.h"
#include "ppbox/demux/base/DemuxEvent.h"
#include "ppbox/demux/base/Demuxer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/logger/Section.h>
#include <framework/timer/Ticker.h>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.DemuxStatistic", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        DemuxStatistic::DemuxStatistic(
            DemuxerBase & demuxer)
            : demuxer_(demuxer)
            , state_(stopped)
            , need_seek_time_(false)
            , seek_position_(0)
            , play_position_(0)
            , block_type_(BlockType::init)
            , last_time_(0)
        {
            ticker_ = new framework::timer::Ticker(1000,true);
            status_infos_.push_back(StatusInfo(stopped, play_position_));
        }

        void DemuxStatistic::update_stat()
        {
            boost::system::error_code ec;
            demuxer_.get_stream_status(*this, ec);

            if (state_ == buffering) {
                LOG_DEBUG("[buf_time] buf_time: " << buf_time() << " ms");
            }

            raise(BufferingEvent(*this));
        }
        /*
        static char const * type_str[] = {
            "stopped", 
            "opening", 
            "opened", 
            "paused",
            "playing", 
            "buffering"
        };
        */
        void DemuxStatistic::change_status(
            boost::uint16_t new_state)
        {
            boost::uint64_t now_time = TimeCounter::elapse();
            boost::uint64_t elapse = now_time - last_time_;
            last_time_ = now_time;
            // ����Buffering״̬��ԭ����ͬ�������м���һ��playing״̬�����ҳ���playʱ�䣨sampleʱ�䣩С��3��
            // ��ô�ϲ�Buffering״̬�����������������playing״̬��Ҳ�ϲ�
            if (state_ == buffering 
                && status_infos_[status_infos_.size() - 1].status_type == status_infos_[status_infos_.size() - 3].status_type 
                && status_infos_[status_infos_.size() - 2].status_type == playing 
                && (play_position_ - status_infos_[status_infos_.size() - 2].play_position < 3000)) {
                    // �ϲ�Buffering״̬���˺�ʣ��һ��Buffering״̬��һ��playing״̬
                    status_infos_[status_infos_.size() - 3].elapse += elapse;
                    status_infos_.pop_back();
                    // �����������playing״̬��Ҳ�ϲ�
                    if (new_state == playing) {
                        last_time_ -= status_infos_.back().elapse;
                        status_infos_.pop_back();
                    }
            } else {
                // �ϲ�ʱ��
                status_infos_.back().elapse = elapse;
            }
            boost::uint16_t type = new_state;
            if (new_state == buffering) {
                type |= block_type_;
            }
            StatusInfo info(type, play_position_);
            status_infos_.push_back(info);
            state_ = (StatusEnum)new_state;

            raise(StatusChangeEvent(*this));
        }

        void DemuxStatistic::open_beg()
        {
            assert(state_ == stopped);
            change_status(opening);
        }

        void DemuxStatistic::open_end()
        {
            assert(state_ <= opening);
            change_status(opened);
            play_position_ = 0;
        }

        void DemuxStatistic::pause()
        {
            if (state_ == paused)
                return;

            change_status(paused);
        }

        void DemuxStatistic::resume()
        {
            if (state_ != paused)
                return;

            change_status(playing);
        }

        void DemuxStatistic::play_on(
            boost::uint64_t sample_time)
        {
            if (ticker_->check()) {
                update_stat();
            }

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

        void DemuxStatistic::block_on()
        {
            if (ticker_->check()) {
                update_stat();
            }

            if (state_ == buffering)
                return;
            change_status(buffering);
        }

        void DemuxStatistic::seek(
            bool ok, 
            boost::uint64_t seek_time)
        {
            // ����϶���ʱ���뵱ǰ���ŵ�ʱ����ͬ���򲻴���
            if (play_position_ == seek_time)
                return;

            time_range.pos = time_range.buf = seek_time;

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

        void DemuxStatistic::last_error(
            boost::system::error_code const & ec)
        {
            last_error_ = ec;
        }

        void DemuxStatistic::close()
        {
            change_status(stopped);
            status_infos_.back().elapse = 0;
            block_type_ = BlockType::init;
        }

    }
}

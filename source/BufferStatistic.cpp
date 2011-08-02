// BufferStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/source/BufferStatistic.h"

#include <framework/timer/Ticker.h>

namespace ppbox
{
    namespace demux
    {

        BufferObserver::BufferObserver()
        {
            stat_.start_time = time(NULL);
            stat_.total_bytes = 0;
            stat_.zero_time = 0;
            stat_.speeds[0].interval = ONE_REG;
            stat_.speeds[1].interval = FIVE_REG;
            stat_.speeds[2].interval = TWENTY_REG;
            stat_.speeds[3].interval = SIXTY_REG;
            for (boost::uint32_t i = 0; i < sizeof(stat_.speeds) / sizeof(SpeedStatistics); i++) {
                stat_.speeds[i].time_left = stat_.speeds[i].interval;
            }
            ticker_ = new framework::timer::Ticker(1000);
        }

        BufferObserver::~BufferObserver()
        {
            delete ticker_;
            ticker_ = NULL;
        }

        boost::uint32_t BufferObserver::get_zero_interval()
        {
            if (stat_.zero_time == 0) {
                return 0;
            } else {
                return (boost::uint32_t)((ticker_->elapse() - stat_.zero_time) / 1000);
            }
        }

        void BufferObserver::reset_zero_interval()
        {
            stat_.zero_time = 0;
        }

        void BufferObserver::increase_download_byte(
            boost::uint32_t byte_size)
        {
            stat_.total_bytes += byte_size; //记录总下载字节数
            boost::uint64_t milli_sec = 0;
            if (ticker_->check(milli_sec)) {
                if ((boost::uint32_t)stat_.total_bytes == stat_.speeds[0].last_bytes) {
                    if (stat_.zero_time == 0)
                        stat_.zero_time = milli_sec;
                } else {
                    stat_.zero_time = 0;
                }
                for (size_t i = 0;i< sizeof(stat_.speeds)/sizeof(SpeedStatistics);i++) {
                    if (--stat_.speeds[i].time_left == 0) {
                        stat_.speeds[i].time_left = stat_.speeds[i].interval;
                        if (milli_sec != stat_.speeds[i].last_milli_sec) {
                            stat_.speeds[i].cur_speed = 
                                (boost::uint32_t)((stat_.total_bytes - stat_.speeds[i].last_bytes) * 1000 / (milli_sec - stat_.speeds[i].last_milli_sec));
                            if (stat_.speeds[i].cur_speed > stat_.speeds[i].peak_speed) {
                                stat_.speeds[i].peak_speed = stat_.speeds[i].cur_speed;
                            }
                        }
                        stat_.speeds[i].last_milli_sec = milli_sec;
                        stat_.speeds[i].last_bytes = stat_.total_bytes;
                    }
                }
            }
        }
     
    } // namespace demux
} // namespace ppbox

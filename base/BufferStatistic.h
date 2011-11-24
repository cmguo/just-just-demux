// BufferStatistic.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_STATISTIC_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_STATISTIC_H_

#include <framework/timer/ClockTime.h>

namespace framework { namespace timer { class Ticker; } }

namespace ppbox
{
    namespace demux
    {
        //定义采样集中需要采样的时间
        static const boost::uint32_t ONE_REG = 1;
        static const boost::uint32_t FIVE_REG = 5;
        static const boost::uint32_t TWENTY_REG = 20;
        static const boost::uint32_t SIXTY_REG = 60;

        struct SpeedStatistics
        {
            SpeedStatistics()
                : interval(0)
                , time_left(0)
                , last_milli_sec(0)
                , last_bytes(0)
                , cur_speed(0)
                , peak_speed(0)
            { 
            }

            boost::uint32_t interval;           // 采样集的类型（秒为单位）
            boost::uint32_t time_left;
            boost::uint64_t last_milli_sec;     // 上一次的采样的时间
            boost::uint64_t last_bytes;
            boost::uint32_t cur_speed;
            boost::uint32_t peak_speed;
        };

        struct BufferStatistic
        {
            BufferStatistic()
                : start_time(0)
                , total_bytes(0)
                , zero_time(0)
            {
            }

            time_t start_time;
            boost::uint64_t total_bytes;        // 记录下载总字节
            boost::uint64_t zero_time;
            SpeedStatistics speeds[4];          // 定义4个采样数据统计集
        };

        class BufferObserver
        {
        public: 
            BufferObserver();

            ~BufferObserver();

        public: 
            //记录每次收到数据的时间间隔
            boost::uint64_t get_current_interval();

            boost::uint32_t get_zero_interval();

            void reset_zero_interval();

            //每次下载数据所调用的统计接口
            void increase_download_byte(
                boost::uint32_t byte_size);

            //获得统计信息
            BufferStatistic const & buffer_stat() const
            {
                return stat_;
            }

        private:
            BufferStatistic stat_;
            framework::timer::Ticker * ticker_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_STATISTIC_H_

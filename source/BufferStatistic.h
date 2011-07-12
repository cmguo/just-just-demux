// BufferStatistic.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_STATISTIC_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_STATISTIC_H_

#include <framework/timer/ClockTime.h>

namespace framework { namespace timer { class Ticker; } }

namespace ppbox
{
    namespace demux
    {
        //�������������Ҫ������ʱ��
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

            boost::uint32_t interval;           // �����������ͣ���Ϊ��λ��
            boost::uint32_t time_left;
            boost::uint64_t last_milli_sec;     // ��һ�εĲ�����ʱ��
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
            boost::uint64_t total_bytes;        // ��¼�������ֽ�
            boost::uint64_t zero_time;
            SpeedStatistics speeds[4];          // ����4����������ͳ�Ƽ�
        };

        class BufferObserver
        {
        public: 
            BufferObserver();

            ~BufferObserver();

        public: 
            //��¼ÿ���յ����ݵ�ʱ����
            boost::uint64_t get_current_interval();

            boost::uint32_t get_zero_interval();

            void reset_zero_interval();

            //ÿ���������������õ�ͳ�ƽӿ�
            void increase_download_byte(
                boost::uint32_t byte_size);

            //���ͳ����Ϣ
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

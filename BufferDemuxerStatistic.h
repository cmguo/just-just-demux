// BufferDemuxerStatistic.h

#ifndef _PPBOX_DEMUX_DEMUXER_STATISTIC_H_
#define _PPBOX_DEMUX_DEMUXER_STATISTIC_H_

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

        class BufferDemuxerStatistic
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

        protected:
            BufferDemuxerStatistic();

        protected:
            void open_beg();

            void open_end();

            void pause();

            void resume();

            void play_on(
                boost::uint32_t sample_time);

            void block_on();

            void seek(
                bool ok, 
                boost::uint32_t seek_time);

            void on_error(
                boost::system::error_code const & ec);

            void close();

            void buf_time(
                boost::uint32_t const & buffer_time);

        public:
            StatusEnum state() const
            {
                return state_;
            }

            boost::uint32_t buf_time() const
            {
                return buffer_time_;
            }

            boost::uint32_t play_position() const
            {
                return play_position_;
            }

            boost::uint32_t seek_position() const
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

        public:
            boost::uint32_t open_total_time() const
            {
                return open_total_time_;
            }

        private:
            void change_status(
                boost::uint16_t new_state);

        protected:
            StatusEnum state_;
            boost::uint32_t buffer_time_;
            bool need_seek_time_;
            boost::uint32_t seek_position_;        // 拖动后的位置（时间毫秒）
            boost::uint32_t play_position_;        // 播放的位置（时间毫秒）
            BlockType::Enum block_type_;
            boost::uint32_t last_time_;
            std::vector<StatusInfo> status_infos_;
            boost::system::error_code last_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_STATISTIC_H_

// RecordDemuxer.h

#ifndef _PPBOX_DEMUX_RECORD_DEMUXER_H_
#define _PPBOX_DEMUX_RECORD_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"

namespace ppbox
{
    namespace demux
    {

        struct Frame
        {
            boost::uint32_t itrack;
            bool sync;
            boost::uint64_t dts;
            boost::uint32_t cts_delta;
            std::vector<boost::uint8_t> data;

            Frame()
                : dts(0)
                , cts_delta(0)
            {
            }

            Frame const & operator=(Frame const & frame)
            {
                itrack = frame.itrack;
                sync   = frame.sync;
                dts    = frame.dts;
                cts_delta = frame.cts_delta;
                data.assign(frame.data.begin(), frame.data.end());
                return *this;
            }

        };

        class RecordDemuxer
            : public DemuxerBase
        {
        public:
            RecordDemuxer(boost::asio::io_service & io_svc)
                : io_svc_(io_svc)
                , step_(StepType::not_open)
                , pool_max_size_(4000)
            {
            }

            ~RecordDemuxer()
            {
            }

        public:
            void set_pool_size(boost::uint32_t size);

            void add_stream(MediaInfo const & info);

            void push(Frame const & frame);

        public:
            void async_open(
                std::string const & name, 
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

            boost::system::error_code cancel(
                boost::system::error_code & ec);

            boost::system::error_code pause(
                boost::system::error_code & ec);

            boost::system::error_code resume(
                boost::system::error_code & ec);

            boost::system::error_code close(
                boost::system::error_code & ec);

            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            virtual boost::uint32_t get_duration(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec);

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec);

        private:
            struct StepType
            {
                enum Enum
                {
                    not_open, 
                    opened, 
                };
            };

        private:
            boost::asio::io_service & io_svc_;
            StepType::Enum step_;
            std::vector<MediaInfo> media_infos_;
            BufferStatistic buffer_statistic_;

            // pool
            boost::uint32_t pool_max_size_;
            std::deque<Frame> pool_;
            Frame cur_frame_;

            boost::mutex mutex_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_RECORD_DEMUXER_H_

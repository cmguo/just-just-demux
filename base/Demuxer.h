// Demuxer.h

#ifndef _PPBOX_DEMUX_DEMUXER_H_
#define _PPBOX_DEMUX_DEMUXER_H_

#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/TimestampHelper.h"

#include <ppbox/common/ClassFactory.h>

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        class Demuxer
            : public DemuxerBase
            , public ppbox::common::ClassFactory<
                Demuxer, 
                std::string, 
                Demuxer * (
                    boost::asio::io_service &, 
                    std::basic_streambuf<boost::uint8_t> &)
            >
        {
        public:
            typedef std::basic_streambuf<boost::uint8_t> streambuffer_t;

        public:
            Demuxer(
                boost::asio::io_service & io_svc, 
                streambuffer_t & buf);

            virtual ~Demuxer();

        public:
            virtual void async_open(
                open_response_type const & resp);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code get_media_info(
                MediaInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

            virtual bool get_data_stat(
                DataStatistic & stat, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual boost::uint64_t check_seek(
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

            virtual bool fill_data(
                boost::system::error_code & ec);

        public:
            void demux_begin(
                TimestampHelper & helper);

            void demux_end();

            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec) const = 0;

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) const = 0;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec) = 0;

        protected:
            virtual boost::uint64_t seek(
                boost::uint64_t & time, 
                boost::uint64_t & delta, // 要重复下载的数据量 
                boost::system::error_code & ec) = 0;

            void on_open();

            void adjust_timestamp(
                Sample & sample)
            {
                helper_->adjust(sample);
            }

            TimestampHelper & time_helper()
            {
                return *helper_;
            }

        private:
            streambuffer_t & buf_;
            TimestampHelper * helper_;
            TimestampHelper default_helper_;
        };

    } // namespace demux
} // namespace ppbox

#define PPBOX_REGISTER_DEMUXER(k, c) PPBOX_REGISTER_CLASS(k, c)

#endif // _PPBOX_DEMUX_DEMUXER_H_

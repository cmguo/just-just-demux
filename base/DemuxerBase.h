// DemuxerBase.h

#ifndef _PPBOX_DEMUX_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_DEMUXER_BASE_H_

#include <ppbox/avformat/Format.h>

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;

        class DemuxerBase
        {
        public:
            DemuxerBase() {};

            virtual ~DemuxerBase() {}

        public:
            virtual boost::system::error_code open(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code close(
                boost::system::error_code & ec) = 0;

            virtual bool is_open(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec) = 0;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) = 0;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) = 0;

            virtual boost::uint64_t seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec) = 0;

            //virtual boost::uint64_t get_offset(
            //    boost::uint64_t & time, 
            //    boost::uint64_t & delta, // 要重复下载的数据量 
            //    boost::system::error_code & ec) = 0;

            //virtual void set_stream(
            //    std::basic_streambuf<boost::uint8_t> & buf) = 0;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_BASE_H_

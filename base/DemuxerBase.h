// DemuxerBase.h

#ifndef _PPBOX_DEMUX_DEMUXER_BASE_H_
#define _PPBOX_DEMUX_DEMUXER_BASE_H_

#include "ppbox/demux/base/TimestampHelper.h"

#include <ppbox/avformat/Format.h>

#include <ppbox/common/Call.h>
#include <ppbox/common/Create.h>

#include <util/smart_ptr/RefenceFromThis.h>

#define PPBOX_REGISTER_DEMUXER(n, c) \
    static ppbox::common::Call reg ## n(ppbox::demux::DemuxerBase::register_demuxer, BOOST_PP_STRINGIZE(n), ppbox::common::Creator<c>())

namespace ppbox
{
    namespace demux
    {

        using ppbox::avformat::StreamInfo;
        using ppbox::avformat::Sample;
        ;
        class DemuxerBase
        {
        public:
            typedef boost::function<DemuxerBase * (
                std::basic_streambuf<boost::uint8_t> & buf)
            > register_type;

        public:
            DemuxerBase(
                std::basic_streambuf<boost::uint8_t> & buf)
            {
            }

            virtual ~DemuxerBase()
            {
            }

            //virtual DemuxerBase * clone(
            //    std::basic_streambuf<boost::uint8_t> & buf)
            //{
            //    return this;
            //}

        public:
            static DemuxerBase * create(
                std::string const & format, 
                std::basic_streambuf<boost::uint8_t> & buf);

            static void register_demuxer(
                std::string const & format,
                register_type func);

            static void destory(
                DemuxerBase *& demuxer);

        public:
            void set_timestamp_helper(
                TimestampHelper & helper)
            {
                helper_ = &helper;
            }

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

            virtual boost::uint64_t get_offset(
                boost::uint64_t & time, 
                boost::uint64_t & delta, // 要重复下载的数据量 
                boost::system::error_code & ec) = 0;

            virtual void set_stream(
                std::basic_streambuf<boost::uint8_t> & buf) = 0;

        protected:
            TimestampHelper & timestamp()
            {
                return *helper_;
            }

        private:
            TimestampHelper * helper_;
            TimestampHelper default_helper_;

        private:
            static std::map<std::string, register_type> & demuxer_map();
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_DEMUXER_BASE_H_

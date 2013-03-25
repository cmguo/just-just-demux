// BasicDemuxer.h

#ifndef _PPBOX_DEMUX_BASIC_BASIC_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_BASIC_DEMUXER_H_

#include "ppbox/demux/base/Demuxer.h"

#include <ppbox/data/base/DataBlock.h>

#include <ppbox/common/ClassFactory.h>

namespace ppbox
{
    namespace demux
    {

        class BasicDemuxer
            : public Demuxer
            , public ppbox::common::ClassFactory<
                BasicDemuxer, 
                std::string, 
                BasicDemuxer * (
                    boost::asio::io_service &, 
                    std::basic_streambuf<boost::uint8_t> &)
            >
        {
        public:
            typedef std::basic_streambuf<boost::uint8_t> streambuffer_t;

        public:
            BasicDemuxer(
                boost::asio::io_service & io_svc, 
                streambuffer_t & buf);

            virtual ~BasicDemuxer();

        public:
            virtual boost::system::error_code get_media_info(
                MediaInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

        public:
            virtual bool free_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
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

        protected:
            void begin_sample(
                Sample & sample)
            {
                datas_.clear();
            }

            void push_data(
                boost::uint64_t off, 
                boost::uint32_t size)
            {
                datas_.push_back(ppbox::data::DataBlock(off, size));
            }

            std::vector<ppbox::data::DataBlock> & datas()
            {
                return datas_;
            }

            void end_sample(
                Sample & sample)
            {
                adjust_timestamp(sample);
                sample.context = &datas_;
            }

        private:
            streambuffer_t & buf_;
            std::vector<ppbox::data::DataBlock> datas_;
        };

    } // namespace demux
} // namespace ppbox

#define PPBOX_REGISTER_BASIC_DEMUXER(k, c) PPBOX_REGISTER_CLASS(k, c)

#endif // _PPBOX_DEMUX_BASIC_BASIC_DEMUXER_H_

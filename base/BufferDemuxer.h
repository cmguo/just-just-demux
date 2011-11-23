// BufferDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_BASE_BUFFER_DEMUXER_H_

#include "ppbox/demux/base/DemuxerError.h"
#include "ppbox/demux/base/DemuxerBase.h"
#include "ppbox/demux/base/DemuxerStatistic.h"

#include <framework/timer/Ticker.h>

#include <boost/function.hpp>

namespace ppbox
{
    namespace demux
    {
        class BufferList;
        class Source;
        class BytesStream;
        
        struct DemuxerEventType
        {
            enum Enum
            {
                seek_segment, 
                seek_statistic,
                play_on,
            };
        };

        class BufferDemuxer
            : public DemuxerStatistic
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            BufferDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size,
                Source * segmentbase);

            virtual ~BufferDemuxer();

        public:
            boost::system::error_code open (
                boost::system::error_code & ec);

            void async_open(
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

            size_t get_media_count(
                boost::system::error_code & ec);

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec);

            boost::uint32_t get_duration(
                boost::system::error_code & ec);

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        protected:
            boost::system::error_code insert_source(
                boost::uint32_t time,
                Source * source, 
                boost::system::error_code & ec);

            boost::system::error_code remove_source(
                Source * source, 
                boost::system::error_code & ec);

        protected:
            void tick_on();

        private:
            void handle_async(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            DemuxerBase * create_demuxer(
                DemuxerType::Enum demuxer_type,
                BytesStream * stream);

            void create_demuxer(
                Source * source, 
                size_t segment, 
                StreamPointer & stream, 
                DemuxerPointer & demuxer, 
                boost::system::error_code & ec);

            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;
            boost::system::error_code extern_error_;
            Source * root_source_;
            BufferList * buffer_;

        private:
            framework::timer::Ticker * ticker_;
            boost::uint32_t seek_time_;

            typedef boost::intrusive_ptr<
                BytesStream> StreamPointer;
            typedef boost::intrusive_ptr<
                DemuxerBase> DemuxerPointer;

            StreamPointer read_stream_;
            StreamPointer write_stream_;
            DemuxerPointer read_demuxer_;
            DemuxerPointer write_demuxer_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BUFFER_DEMUXER_H_

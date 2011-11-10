// BufferDemuxer.h

#ifndef _PPBOX_DEMUX_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_BUFFER_DEMUXER_H_

#include "ppbox/demux/DemuxerError.h"
#include "ppbox/demux/DemuxerBase.h"

#include <boost/function.hpp>

namespace ppbox
{
    namespace demux
    {
        class BufferList;
        class SegmentsBase;
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
                SegmentsBase * segmentbase);

            virtual ~BufferDemuxer();

        public:
            boost::system::error_code open (
                boost::system::error_code & ec);

            void async_open(
                open_response_type const & resp);

            bool is_open(
                boost::system::error_code & ec);

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            boost::system::error_code get_sample(
                Sample & sample, 
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

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec);

        public:
            virtual void on_event(
                DemuxerEventType::Enum event_type,
                void const * const arg,
                boost::system::error_code & ec)
            {
            }

        private:
            void handle_async(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

            DemuxerBase * create_demuxer(
                DemuxerType::Enum demuxer_type,
                BytesStream * buf);

            void create_demuxer(
                boost::system::error_code & ec);

            void create_segment_demuxer(
                size_t segment_,
                boost::system::error_code & ec);

            void create_write_demuxer(
                boost::system::error_code & ec);

            void create_read_demuxer(
                boost::system::error_code & ec);

        protected:
            BufferList * buffer_;
            boost::uint32_t seek_time_;
            boost::system::error_code pending_error_;

        private:
            SegmentsBase * segments_;
            BytesStream * read_stream_;
            BytesStream * write_stream_;
            DemuxerBase * read_demuxer_;
            DemuxerBase * write_demuxer_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BUFFER_DEMUXER_H_
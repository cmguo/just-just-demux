// PptvDemuxer.h

#ifndef _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_
#define _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_

#include "ppbox/demux/base/BufferDemuxer.h"

#include "ppbox/demux/pptv/PptvDemuxerStatistic.h"

namespace framework { namespace timer { class Ticker; } }
namespace framework { namespace network { class NetName; } }

namespace ppbox
{
    namespace demux
    {

        struct MediaInfo;

        struct Sample;

        class SourceBase;

        struct SegmentInfo
        {
            size_t index;
            boost::uint32_t duration;
            boost::uint32_t duration_offset;
            boost::uint64_t file_length;
            boost::uint64_t head_length;
            std::vector<boost::uint8_t> head_data;
        };

        class PptvDemuxer
            : public BufferDemuxer
            , public PptvDemuxerStatistic
        {

        public:
            PptvDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size,
                SourceBase * segmentbase);

            virtual ~PptvDemuxer();

        public:
            virtual boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec);

            virtual void async_open(
                std::string const & name, 
                open_response_type const & resp) = 0;

            virtual boost::system::error_code set_http_proxy(
                framework::network::NetName const & addr, 
                boost::system::error_code & ec);

            virtual boost::system::error_code set_max_dl_speed(
                boost::uint32_t speed, // KBps
                boost::system::error_code & ec);

        public:
            boost::system::error_code get_sample_buffered(
                Sample & sample, 
                boost::system::error_code & ec);

            boost::uint32_t get_buffer_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf);

            void on_extern_error(
                boost::system::error_code const & ec);

        private:
            void update_stat();

        protected:
            boost::asio::io_service & io_svc_;
            boost::system::error_code extern_error_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_PPTV_PPTV_DEMUXER_H_

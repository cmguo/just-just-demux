// EmptyDemuxer.h

#ifndef _PPBOX_DEMUX_EMPTY_DEMUXER_H_
#define _PPBOX_DEMUX_EMPTY_DEMUXER_H_

#include "ppbox/demux/base/BufferDemuxer.h"

namespace ppbox
{
    namespace demux
    {
        class EmptyDemuxer
            : public PptvDemuxer
        {
        public:
            EmptyDemuxer(
                boost::asio::io_service & io_svc)
                : PptvDemuxer(io_svc, 0, 0, NULL)
            {
            }

            ~EmptyDemuxer()
            {
                if (buffer_stat_) {
                    delete buffer_stat_;
                    buffer_stat_ = NULL;
                }
            }

        public:
            boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec)
            {
                ec = ppbox::demux::error::bad_file_type;
                return ec;
            }

            void async_open(
                std::string const & name, 
                open_response_type const & resp)
            {
                boost::system::error_code ec;
                open(name, ec);
                io_svc_.post(boost::bind(resp, ec));
            }

            bool is_open(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return false;
            };

            boost::system::error_code cancel(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::system::error_code pause(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::system::error_code resume(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::system::error_code close(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            size_t get_media_count(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return 0;
            }

            boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::uint32_t get_duration(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return 0;
            }

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                ec = framework::system::logic_error::not_supported;
                ec_buf =framework::system::logic_error::not_supported;
                return 0;
            }

            boost::uint32_t get_cur_time(
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return 0;
            }

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

            boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)
            {
                ec = framework::system::logic_error::not_supported;
                return ec;
            }

        private:
            BufferStatistic * buffer_stat_;
        };
    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_EMPTY_DEMUXER_H_


// CommonDemuxer.h

#ifndef _PPBOX_DEMUX_COMMON_DEMUXER_H_
#define _PPBOX_DEMUX_COMMON_DEMUXER_H_

#include "ppbox/demux/Demuxer.h"

namespace ppbox
{
    namespace demux
    {

        template <
            typename Buffer, 
            typename BufferDemuxer
        >
        class CommonDemuxer
            : private Buffer
            , private BufferDemuxer
            , public Demuxer
        {
        public:
            CommonDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : Buffer(io_svc, buffer_size, prepare_size)
                , BufferDemuxer((Buffer &)(*this))
                , Demuxer(io_svc, Buffer::buffer_stat())
            {
            }

            virtual ~CommonDemuxer()
            {
            }

        public:
            virtual boost::system::error_code open(
                std::string const & name, 
                boost::system::error_code & ec)
            {
                std::vector<std::string> key_playlink;
                slice<std::string>(name, std::inserter(
                    key_playlink, key_playlink.end()), "|");
                assert(key_playlink.size() > 0);
                std::string playlink = key_playlink[key_playlink.size()-1];
                Demuxer::open_beg(playlink);
                Buffer::set_name(playlink);
                BufferDemuxer::open(ec);
                if (!ec) {
                    BufferDemuxer::is_open(ec);
                }
                Demuxer::open_end(ec);
                return ec;
            }

            virtual void async_open(
                std::string const & name, 
                Demuxer::open_response_type const & resp)
            {
                boost::system::error_code ec;
                open(name, ec);
                io_svc_.post(boost::bind(resp, ec));
            }

            virtual bool is_open(
                boost::system::error_code & ec)
            {
                return BufferDemuxer::is_open(ec);
            }

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec)
            {
                return Buffer::cancel(ec);
            }

            virtual boost::system::error_code pause(
                boost::system::error_code & ec)
            {
                DemuxerStatistic::pause();
                ec.clear();
                return ec;
            }

            virtual boost::system::error_code resume(
                boost::system::error_code & ec)
            {
                ec.clear();
                return ec;
            }

            virtual boost::system::error_code close(
                boost::system::error_code & ec)
            {
                DemuxerStatistic::close();
                return Buffer::close(ec);
            }

            virtual size_t get_media_count(
                boost::system::error_code & ec)
            {
                return BufferDemuxer::get_media_count(ec);
            }

            virtual boost::system::error_code get_media_info(
                size_t index, 
                MediaInfo & info, 
                boost::system::error_code & ec)
            {
                return BufferDemuxer::get_media_info(index, info, ec);
            }

            virtual boost::uint32_t get_duration(
                boost::system::error_code & ec)
            {
                return BufferDemuxer::get_duration(ec);
            }

            virtual boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec)
            {
                BufferDemuxer::seek(time, ec);
                DemuxerStatistic::seek(time, ec);
                return ec;
            }

            virtual boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                if ((ec = extern_error_)) {
                    return 0;
                } else {
                    if (Buffer::write_segment() > 0) {
                        ec.clear();
                        ec_buf = error::no_more_sample;
                        return BufferDemuxer::get_duration(ec);
                    } else {
                        return BufferDemuxer::get_end_time(ec, ec_buf);
                    }
                }
            }

            virtual boost::uint32_t get_cur_time(
                boost::system::error_code & ec)
            {
                return BufferDemuxer::get_cur_time(ec);
            }

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec)
            {
                if ((ec = extern_error_)) {
                } else {
                    BufferDemuxer::get_sample(sample, ec);
                }
                if (!ec) {
                    DemuxerStatistic::play_on(sample.time);
                } else if (ec == boost::asio::error::would_block) {
                    DemuxerStatistic::block_on();
                }
                return ec;
            }

            virtual boost::system::error_code set_non_block(
                bool non_block, 
                boost::system::error_code & ec)
            {
                return Buffer::set_non_block(non_block, ec);
            }

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)
            {
                return Buffer::set_time_out(time_out, ec);
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_COMMON_DEMUXER_H_

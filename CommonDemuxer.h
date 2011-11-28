// CommonDemuxer.h

#ifndef _PPBOX_DEMUX_COMMON_DEMUXER_H_
#define _PPBOX_DEMUX_COMMON_DEMUXER_H_

#include "ppbox/demux/PptvDemuxer.h"
#include "ppbox/demux/source/BufferList.h"

namespace ppbox
{
    namespace demux
    {

        template <
            typename Segments
        >
        class CommonDemuxer
            : public PptvDemuxer
        {
        public:
            CommonDemuxer(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size,
                DemuxerType::Enum demuxer_type)
                : PptvDemuxer(io_svc, buffer_size, prepare_size, segments_ = new Segments(io_svc, demuxer_type))
            {
                segments_->set_buffer_list(BufferDemuxer::buffer_);
                segments_->set_demuxer_type(demuxer_type);
            }

            virtual ~CommonDemuxer()
            {
                delete segments_;
                delete buffer_;
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
                PptvDemuxer::open_beg(playlink);
                segments_->set_name(playlink);
                BufferDemuxer::open(ec);
                if (!ec) {
                    BufferDemuxer::is_open(ec);
                }
                PptvDemuxer::open_end(ec);
                return ec;
            }

            virtual void async_open(
                std::string const & name, 
                PptvDemuxer::open_response_type const & resp)
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
                return buffer_->cancel(ec);
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
                return buffer_->close(ec);
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
                    if (buffer_->write_segment() > 0) {
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
                return segments_->set_non_block(non_block, ec);
            }

            virtual boost::system::error_code set_time_out(
                boost::uint32_t time_out, 
                boost::system::error_code & ec)
            {
                return segments_->set_time_out(time_out, ec);
            }

        private:
            BufferList * buffer_;
            Segments * segments_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_COMMON_DEMUXER_H_

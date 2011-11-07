// FlvBufferDemuxer.h

#ifndef _PPBOX_DEMUX_FLV_FLV_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_FLV_FLV_BUFFER_DEMUXER_H_

#include "ppbox/demux/flv/FlvDemuxerBase.h"
#include "ppbox/demux/source/BytesStream.h"

#include <boost/asio/io_service.hpp>
#include <boost/type_traits/remove_const.hpp>

namespace ppbox
{
    namespace demux
    {

        template <typename Buffer>
        class FlvBufferDemuxer
            : public FlvDemuxerBase
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            FlvBufferDemuxer(
                Buffer & buffer)
                : FlvDemuxerBase(*(stream_buf_ = new BytesStream<Buffer>(buffer)))
                , buffer_(buffer)
                , segment_(0)
            {
            }

            ~FlvBufferDemuxer()
            {
                if (stream_buf_) {
                    delete stream_buf_;
                    stream_buf_ = NULL;
                }
            }

            boost::system::error_code open(
                boost::system::error_code & ec)
            {
                segment_ = buffer_.add_segment(ec);
                stream_buf_->more(0);
                FlvDemuxerBase::open(ec);
                if (!ec) {
                    stream_buf_->drop();
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    ec = stream_buf_->error();
                    if (ec == boost::asio::error::eof) {
                        FlvDemuxerBase::open(ec);
                    }
                }
                return ec;
            }

            void async_open(
                open_response_type const & resp)
            {
                resp_ = resp;

                boost::system::error_code ec;
                segment_ = buffer_.add_segment(ec);

                handle_async(boost::system::error_code());
            }

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec)
            {
                stream_buf_->more(0);
                FlvDemuxerBase::get_sample(sample, ec);
                if (!ec) {
                    stream_buf_->drop();
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    if (buffer_.read_segment() != buffer_.write_segment()) {    // 当前分段已经下载完成
                        stream_buf_->drop_all();
                        FlvDemuxerBase::open(ec) 
                           || FlvDemuxerBase::get_sample(sample, ec);
                        if (ec == ppbox::demux::error::file_stream_error) {
                            ec = stream_buf_->error();
                        }
                    } else {
                        ec = stream_buf_->error();
                    }
                }
                return ec;
            }

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                stream_buf_->more(0);
                ec_buf = stream_buf_->error();

                if (!is_open(ec)) {
                    if (ec == ppbox::demux::error::file_stream_error) {
                        ec = stream_buf_->error();
                    }
                    return 0;
                }

                return 0;
            }

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec)
            {
                boost::uint64_t offset = FlvDemuxerBase::seek_to(time, ec);
                if (!ec) {
                    buffer_.seek(segment_, offset, ec);
                }
                return ec;
            }

        private:
            void handle_async(
                boost::system::error_code const & ecc)
            {
                boost::system::error_code ec = ecc;
                if (!ec) {
                    stream_buf_->update_new();

                    FlvDemuxerBase::open(ec);
                    if (!ec) {
                        stream_buf_->drop();
                    } else if (ec == ppbox::demux::error::file_stream_error) {
                        //ec = asf_buf_->error();

                        buffer_.async_prepare_at_least(0, 
                            boost::bind(&FlvBufferDemuxer::handle_async, this, _1));

                        return;
                    }
                }

                response(ec);
            }

            void response(
                boost::system::error_code const & ec)
            {
                open_response_type resp;
                resp.swap(resp_);
                resp(ec);
            }

        private:
            BytesStream<Buffer> * stream_buf_;
            Buffer & buffer_;
            size_t segment_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_BUFFER_DEMUXER_H_

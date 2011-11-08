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
                : FlvDemuxerBase(*(stream_ = new BytesStream<Buffer>(buffer)))
                , buffer_(buffer)
                , segment_(0)
            {
            }

            ~FlvBufferDemuxer()
            {
                if (stream_) {
                    delete stream_;
                    stream_ = NULL;
                }
            }

            boost::system::error_code open(
                boost::system::error_code & ec)
            {
                segment_ = buffer_.add_segment(ec);
                stream_->more(0);
                FlvDemuxerBase::open(ec);
                if (!ec) {
                    stream_->drop();
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    ec = stream_->error();
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
                FlvDemuxerBase::open(ec);
                handle_async(boost::system::error_code());
            }

            boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec)
            {
                stream_->more(0);
                stream_->drop();
                while (FlvDemuxerBase::get_sample(sample, ec)) {
                    if (ec == ppbox::demux::error::file_stream_error) {
                        if (buffer_.read_segment() != buffer_.write_segment()) {    // 当前分段已经下载完成
                            std::cout << "drop_all" << std::endl;
                            stream_->drop_all();
                            FlvDemuxerBase::open(ec); 
                        } else {
                            ec = stream_->error();
                            break;
                        }
                    }
                }
                if (!ec) {
                    sample.data.clear();
                    for(std::vector<FileBlock>::iterator iter = sample.blocks.begin();
                        iter != sample.blocks.end();
                        iter++) {
                            buffer_.peek((*iter).offset, (*iter).size, sample.data, ec);
                            if (ec) {
                                break;
                            }
                    }
                }
                return ec;
            }

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                stream_->more(0);
                ec_buf = stream_->error();

                if (is_open(ec)) {
                    return buffer_.write_segment() * 5000;
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
                    stream_->update_new();
                    if (FlvDemuxerBase::is_open(ec)) {
                        stream_->drop();
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
            BytesStream<Buffer> * stream_;
            Buffer & buffer_;
            size_t segment_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_FLV_FLV_BUFFER_DEMUXER_H_

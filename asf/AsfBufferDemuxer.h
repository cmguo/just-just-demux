// AsfBufferDemuxer.h

#ifndef _PPBOX_DEMUX_ASF_ASF_BUFFER_DEMUXER_H_
#define _PPBOX_DEMUX_ASF_ASF_BUFFER_DEMUXER_H_

#include "ppbox/demux/asf/AsfDemuxerBase.h"
#include "ppbox/demux/source/BytesStream.h"

#include <framework/timer/TickCounter.h>

#include <boost/asio/io_service.hpp>
#include <boost/type_traits/remove_const.hpp>

namespace ppbox
{
    namespace demux
    {

        template <typename Buffer>
        class AsfBufferDemuxer
            : public AsfDemuxerBase
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &)
            > open_response_type;

        public:
            AsfBufferDemuxer(
                Buffer & buffer)
                : AsfDemuxerBase(*(asf_buf_ = new BytesStream<Buffer>(buffer)))
                , buffer_(buffer)
                , segment_(0)
            {
            }

            ~AsfBufferDemuxer()
            {
                if (asf_buf_) {
                    delete asf_buf_;
                    asf_buf_ = NULL;
                }
            }

            boost::system::error_code open(
                boost::system::error_code & ec)
            {
                segment_ = buffer_.add_segment(ec);
                asf_buf_->more(0);
                AsfDemuxerBase::open(ec);
                if (!ec) {
                    asf_buf_->drop();
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    ec = asf_buf_->error();
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
                asf_buf_->more(0);
                asf_buf_->drop();
                AsfDemuxerBase::get_sample(sample, ec);
                if (!ec) {
                    sample.data.clear();
                    for(std::vector<FileBlock>::iterator iter = sample.blocks.begin();
                        iter != sample.blocks.end();
                        iter++) {
                            std::deque<boost::asio::const_buffer> datas;
                            buffer_.peek((*iter).offset, (*iter).size, datas, ec);
                            if (!ec) {
                                for (boost::uint32_t i = 0; i < datas.size(); i++) {
                                    sample.data.push_back(datas[i]);
                                }
                            } else {
                                break;
                            }
                    }
                } else if (ec == ppbox::demux::error::file_stream_error) {
                    if (buffer_.read_segment() != buffer_.write_segment()) {    // 当前分段已经下载完成
                        std::cout << "drop_all" << std::endl;
                        asf_buf_->drop_all();
                        AsfDemuxerBase::open(ec) 
                            || AsfDemuxerBase::get_sample(sample, ec);
                        if (ec == ppbox::demux::error::file_stream_error) {
                            ec = asf_buf_->error();
                        }
                    } else {
                        ec = asf_buf_->error();
                    }
                }
                return ec;
            }

            boost::uint32_t get_end_time(
                boost::system::error_code & ec, 
                boost::system::error_code & ec_buf)
            {
                asf_buf_->more(0);
                ec_buf = asf_buf_->error();
                boost::uint32_t buffer_time = AsfDemuxerBase::get_end_time(ec);
                if (ec == ppbox::demux::error::file_stream_error) {
                    ec = asf_buf_->error();
                }
                return buffer_time;
            }

            boost::system::error_code seek(
                boost::uint32_t & time, 
                boost::system::error_code & ec)
            {
                boost::uint64_t offset = AsfDemuxerBase::seek_to(time, ec);
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
                    asf_buf_->update_new();

                    AsfDemuxerBase::open(ec);
                    if (!ec) {
                        asf_buf_->drop();
                    } else if (ec == ppbox::demux::error::file_stream_error) {
                        //ec = asf_buf_->error();

                        buffer_.async_prepare_at_least(0, 
                            boost::bind(&AsfBufferDemuxer::handle_async, this, _1));

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
            BytesStream<Buffer> * asf_buf_;
            Buffer & buffer_;
            size_t segment_;

            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_ASF_ASF_BUFFER_DEMUXER_H_

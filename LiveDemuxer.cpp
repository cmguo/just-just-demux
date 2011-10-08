// LiveDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/LiveDemuxer.h"
#include "ppbox/demux/PptvJump.h"
#include "ppbox/demux/LiveSegments.h"
#include "ppbox/demux/asf/AsfBufferDemuxer.h"
using namespace ppbox::demux::error;

#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h> 

#include <framework/string/Format.h>
#include <framework/string/FormatStl.h>
#include <framework/timer/Timer.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::timer;
using namespace framework::string;
using namespace framework::logger;
using namespace framework::system::logic_error;

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("LiveDemuxer", 0)

namespace ppbox
{
    namespace demux
    {

        class LiveDemuxerImpl
            : public AsfBufferDemuxer<LiveSegments>
        {
        public:
            LiveDemuxerImpl(
                LiveSegments & buf)
                : AsfBufferDemuxer<LiveSegments>(buf)
            {
            }
        };

        LiveDemuxer::LiveDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint16_t live_port, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size)
            : Demuxer(io_svc, (buffer_ = new LiveSegments(io_svc, live_port, buffer_size, prepare_size))->buffer_stat())
            , demuxer_(new LiveDemuxerImpl(*buffer_))
            , jump_(new PptvJump(io_svc, JumpType::live))
            , open_step_(StepType::not_open)
        {
            set_play_type(DemuxerType::live);
            buffer_->set_live_demuxer(this);
        }

        LiveDemuxer::~LiveDemuxer()
        {
            if (jump_) {
                delete jump_;
                jump_ = NULL;
            }
            if (demuxer_) {
                delete demuxer_;
                demuxer_ = NULL;
            }
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
        }

        void LiveDemuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            name_ = name;
            resp_ = resp;

            open_step_ = StepType::opening;

            handle_async_open(boost::system::error_code());
        }

        void LiveDemuxer::open_logs_end(
            ppbox::common::HttpStatistics const & http_stat, 
            int index, 
            error_code const & ec)
        {
            open_logs_[index] = http_stat;
            open_logs_[index].total_elapse = open_logs_[index].elapse();
            open_logs_[index].last_last_error = ec;
        }

        static char const * type_str[] = {
            "not_open", 
            "opening", 
            "jump", 
            "head_normal", 
            "finish",
        };

        void LiveDemuxer::handle_async_open(
            error_code const & ecc)
        {
            LOG_S(Logger::kLevelDebug, "[handle_async_open] step: " 
                << type_str[open_step_] << ", ec: " << ecc.message());

            if (ecc && open_step_ == StepType::jump) {
                open_logs_end(jump_->http_stat(), 0, ecc);

                LOG_S(Logger::kLevelDebug, "[handle_async_open] jump ec: " << ecc.message());

                LOG_S(Logger::kLevelAlarm, "jump: failure");

                open_step_ = StepType::head_normal;

                buffer_->set_max_try(1);

                demuxer_->async_open(
                    boost::bind(&LiveDemuxer::handle_async_open, this, _1));
                return;
            }

            error_code ec = ecc;
            if (ec) {
                if (ec != boost::asio::error::would_block) {
                    if (open_step_ == StepType::head_normal) {
                        LOG_S(Logger::kLevelAlarm, "data: failure");
                        //seg_end(buffer_->segment());
                    }

                    open_end(ec);
                }

                is_ready_ = true;
                response(ec);
                return;
            }

            switch (open_step_) {
            case StepType::opening:
                {
                    Demuxer::open_beg(name_);

                    open_logs_.resize(2);

                    std::string::size_type slash = name_.find('|');
                    if (slash == std::string::npos) {
                        ec = empty_name;
                    } else {
                        buffer_->set_url_key(name_.substr(0, slash), name_.substr(slash + 1));

                        if (buffer_->get_name().empty()) {
                            ec = empty_name;
                        }
                    }

                    if (!ec) {
                        LOG_S(Logger::kLevelDebug, "Channel name: " << buffer_->get_name());
                        LOG_S(Logger::kLevelDebug, "Channel uuid: " << buffer_->get_uuid());
                        demux_data().set_name(buffer_->get_name());

                        open_logs_[0].reset();

                        open_step_ = StepType::jump;

                        LOG_S(Logger::kLevelEvent, "jump: start");

                        jump_->async_get(
                            buffer_->get_jump_url(), 
                            boost::bind(&LiveDemuxer::handle_async_open, this, _1));
                        return;
                    }
                    break;
                }
            case StepType::jump:
                {
                    LiveJumpInfo jump_info;
                    parse_jump(jump_info, jump_->get_buf(), ec);
                    open_logs_end(jump_->http_stat(), 0, ec);

                    if (!ec) {
                        set_info_by_jump(jump_info);

                        open_step_ = StepType::head_normal;

                        buffer_->set_max_try(1);

                        demuxer_->async_open(
                            boost::bind(&LiveDemuxer::handle_async_open, this, _1));
                        return;
                    } else {
                        LOG_S(Logger::kLevelDebug, "[handle_async_open] jump ec: " << ec.message());

                        LOG_S(Logger::kLevelAlarm, "jump: failure");
                    }
                    break;
                }
            case StepType::head_normal:
                {
                    LOG_S(Logger::kLevelEvent, "data: success");

                    seg_end(buffer_->segment());
                    LOG_S(Logger::kLevelDebug, "open asf head succeed (" << open_logs_[1].total_elapse << " milliseconds)");

                    open_step_ = StepType::finish;
                    break;
                }
            default:
                assert(0);
                return;
            }

            if (ec != boost::asio::error::would_block) {
                open_end(ec);
            }

            is_ready_ = true;

            response(ec);
        }

        void LiveDemuxer::parse_jump(
            LiveJumpInfo & jump_info,
            boost::asio::streambuf & buf, 
            error_code & ec)
        {
            LOG_S(Logger::kLevelEvent, "jump: success");

            if (!ec) {
                std::string buffer = boost::asio::buffer_cast<char const *>(buf.data());
                LOG_S(Logger::kLevelDebug2, "[parse_jump] jump buffer: " << buffer);

                util::archive::XmlIArchive<> ia(buf);
                ia >> jump_info;
                if (!ia) {
                    ec = bad_file_format;
                }
            }
        }

        void LiveDemuxer::set_info_by_jump(
            LiveJumpInfo & jump_info)
        {
            LOG_S(Logger::kLevelDebug, "jump succeed (" << open_logs_[0].total_elapse << " milliseconds)");
            LOG_S(Logger::kLevelDebug, "proto_type: " << jump_info.proto_type);
            LOG_S(Logger::kLevelDebug, "buffer_size: " << jump_info.buffer_size);
            demux_data().set_server_host(format(jump_info.server_hosts));
            for (size_t i = 0; i < jump_info.server_hosts.size(); ++i) {
                LOG_S(Logger::kLevelDebug, "server host: " << jump_info.server_hosts[i]);
            }
            buffer_->set_jump_info(jump_info);
        }

        void LiveDemuxer::response(
            error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);

            io_svc_.post(boost::bind(resp, ec));
        }

        bool LiveDemuxer::is_open(
            error_code & ec)
        {
            return demuxer_->is_open(ec);
        }

        error_code LiveDemuxer::cancel(
            error_code & ec)
        {
            jump_->cancel();
            return buffer_->cancel(ec);
        }

        error_code LiveDemuxer::pause(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code LiveDemuxer::resume(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code LiveDemuxer::close(
            error_code & ec)
        {
            DemuxerStatistic::close();
            return buffer_->close(ec);
        }

        size_t LiveDemuxer::get_media_count(
            error_code & ec)
        {
            size_t count = demuxer_->get_media_count(ec);
            if (!ec) {
                if (jump_) {
                    delete jump_;
                    jump_ = NULL;
                }
            }
            return count;
        }

        error_code LiveDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            error_code & ec)
        {
            demuxer_->get_media_info(index, info, ec);
            if (!ec) {
                if (jump_) {
                    delete jump_;
                    jump_ = NULL;
                }
            }
            return ec;
        }

        boost::uint32_t LiveDemuxer::get_duration(
            error_code & ec)
        {
            ec = not_supported;
            return 0;
        }

        boost::uint32_t LiveDemuxer::get_end_time(
            error_code & ec, 
            error_code & ec_buf)
        {
            tick_on();
            if ((ec = extern_error_)) {
                return 0;
            } else {
                return demuxer_->get_end_time(ec, ec_buf);
            }
        }

        boost::uint32_t LiveDemuxer::get_cur_time(
            error_code & ec)
        {
            return demuxer_->get_cur_time(ec);
        }

        error_code LiveDemuxer::seek(
            boost::uint32_t time, 
            error_code & ec)
        {
            ec = not_supported;
            return ec;
        }

        error_code LiveDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            tick_on();
            if ((ec = extern_error_)) {
            } else {
                demuxer_->get_sample(sample, ec);

                if (ec == would_block) {
                    block_on();
                } else {
                    play_on(sample.time);
                }
            }
            //while (ec && ec != boost::asio::error::would_block) {
            //    demuxer_->close(ec);
            //    buffer_->clear();
            //    demuxer_->open(ec);
            //}
            return ec;
        }

        error_code LiveDemuxer::set_non_block(
            bool non_block, 
            error_code & ec)
        {
            return buffer_->set_non_block(non_block, ec);
        }

        error_code LiveDemuxer::set_time_out(
            boost::uint32_t time_out, 
            error_code & ec)
        {
            return buffer_->set_time_out(time_out, ec);
        }

        error_code LiveDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            error_code & ec)
        {
            ec = error_code();
            buffer_->set_http_proxy(addr);
            return ec;
        }

        void LiveDemuxer::seg_beg(
            size_t segment)
        {
            if (segment == 0) {
                open_logs_[1].begin_try();
            }
        }

        void LiveDemuxer::seg_end(
            size_t segment)
        {
            if (segment == 0) {
                open_logs_[1].end_try(buffer_->http_stat());
                if (open_logs_[1].try_times == 1)
                    open_logs_[1].response_data_time = open_logs_[1].total_elapse;
            }
        }

    }
}

// Live3Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/Live3Demuxer.h"
#include "ppbox/demux/PptvPlay.h"
#include "ppbox/demux/Live3Segments.h"
#include "ppbox/demux/flv/FlvBufferDemuxer.h"
using namespace ppbox::demux::error;

#include <util/buffers/BufferCopy.h>
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

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Live3Demuxer", 0)

namespace ppbox
{
    namespace demux
    {

        class Live3DemuxerImpl
            : public FlvBufferDemuxer<Live3Segments>
        {
        public:
            Live3DemuxerImpl(
                Live3Segments & buf)
                : FlvBufferDemuxer<Live3Segments>(buf)
            {
            }
        };

        Live3Demuxer::Live3Demuxer(
            boost::asio::io_service & io_svc, 
            boost::uint16_t live_port, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size)
            : Demuxer(io_svc, (buffer_ = new Live3Segments(io_svc, live_port, buffer_size, prepare_size))->buffer_stat())
            , demuxer_(new Live3DemuxerImpl(*buffer_))
            , play_(new PptvPlay(io_svc))
            , open_step_(StepType::not_open)
            , time_(0)
        {
            set_play_type(DemuxerType::live3);
            buffer_->set_live_demuxer(this);
        }

        Live3Demuxer::~Live3Demuxer()
        {
            if (demuxer_) {
                delete demuxer_;
                demuxer_ = NULL;
            }
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
        }

        void Live3Demuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            Demuxer::open_beg(name);
            open_logs_.resize(2);
            
            buffer_->set_id(name);
            resp_ = resp;

            if( parse(name) )
            {
                open_step_ = StepType::head_normal;
                demuxer_->async_open(
                    boost::bind(&Live3Demuxer::handle_async_open, this, _1));
                return;
            }
            else
            {
                open_step_ = StepType::opening;
            }
            handle_async_open(error_code());
        }

        bool Live3Demuxer::parse(const std::string name)
        {
            std::string::size_type slash = name.find('|');
            boost::system::error_code ec;
            if (slash == std::string::npos) 
            {
                return false;
            } 
            std::string key = name.substr(0, slash);
            std::string url = name.substr(slash + 1);

            framework::string::Url request_url(url);
            url = request_url.path().substr(1);

            Live2PlayInfo play_info;
            //ft
            std::string tmp_param = request_url.param("f");
            if(tmp_param.empty())
            { //ÂëÁ÷
                tmp_param = request_url.param("ft");
                if(tmp_param.empty())
                {
                    LOG_S(Logger::kLevelDebug, "parse ft or f failed");
                    return false;
                }
            }
            parse2(tmp_param.c_str(), play_info.dt.bwt);
            
            //name
            tmp_param = request_url.param("name"); //rid
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse name failed");
                return false;
            }
            play_info.channel.stream.video.rid = name;

            //server host
            tmp_param = request_url.param("svrhost");
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse svrhost failed");
                return false;
            }

            framework::network::NetName svhost(tmp_param.c_str());
            play_info.dt.sh = svhost;

            //server time
            tmp_param = request_url.param("svrtime");
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse svrtime failed");
                return false;
            }
            parse2(tmp_param.c_str(), play_info.dt.st_t);

            //delay time
            tmp_param = request_url.param("delaytime");
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse delaytime failed");
                return false;
            }
            parse2(tmp_param.c_str(), play_info.channel.stream.delay);

            //bitrate
            tmp_param = request_url.param("bitrate");
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse bitrate failed");
                return false;
            }
            parse2(tmp_param.c_str(), play_info.channel.stream.video.bitrate);

            //interval
            tmp_param = request_url.param("interval");
            if(tmp_param.empty())
            {
                LOG_S(Logger::kLevelDebug, "parse interval failed");
                return false;
            }

            parse2(tmp_param.c_str(), play_info.channel.stream.interval);

            buffer_->set_play_info(play_info);

            return true;
        }

        void Live3Demuxer::open_logs_end(
            ppbox::common::HttpStatistics const & http_stat, 
            int index, 
            error_code const & ec)
        {
            if (&http_stat != &open_logs_[index])
                open_logs_[index] = http_stat;
            open_logs_[index].total_elapse = open_logs_[index].elapse();
            open_logs_[index].last_last_error = ec;
        }

        void Live3Demuxer::handle_async_open(
            error_code const & ecc)
        {
            error_code ec = ecc;
            if (ec) {
                if (ec != boost::asio::error::would_block) {
                    if (open_step_ == StepType::play) {
                        LOG_S(Logger::kLevelAlarm, "play: failure");
                        open_logs_end(play_->http_stat(), 0, ec);
                        LOG_S(Logger::kLevelDebug, "play failure (" << open_logs_[0].total_elapse << " milliseconds)");
                    }

                    if (open_step_ == StepType::head_normal) {
                        LOG_S(Logger::kLevelAlarm, "data: failure");
                        open_logs_end(open_logs_[1], 1, ec);
                        LOG_S(Logger::kLevelDebug, "data failure (" << open_logs_[1].total_elapse << " milliseconds)");
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
                    if (buffer_->get_id().empty()) {
                        ec = empty_name;
                    }

                    if (!ec) {
                        LOG_S(Logger::kLevelDebug, "Channel name: " << buffer_->get_id());
                        LOG_S(Logger::kLevelDebug, "Channel uuid: " << buffer_->get_uuid());
                        demux_data().set_name(buffer_->get_id());

                        open_logs_[0].reset();

                        open_step_ = StepType::play;

                        LOG_S(Logger::kLevelEvent, "play: start");

                        play_->async_get(
                            buffer_->get_play_url(), 
                            buffer_->get_http_proxy(),
                            boost::bind(&Live3Demuxer::handle_async_open, this, _1));
                        return;
                    }
                    break;
                }
            case StepType::play:
                {
                    Live2PlayInfo play_info;
                    parse_play(play_info, play_->get_buf(), ec);
                    open_logs_end(play_->http_stat(), 0, ec);
                    LOG_S(Logger::kLevelDebug, "play used (" << open_logs_[0].total_elapse << " milliseconds)");

                    if (!ec) {
                        LOG_S(Logger::kLevelEvent, "play: success");

                        set_info_by_play(play_info);

                        open_step_ = StepType::head_normal;

                        demuxer_->async_open(
                            boost::bind(&Live3Demuxer::handle_async_open, this, _1));
                        return;
                    } else {
                        LOG_S(Logger::kLevelDebug, "play ec: " << ec.message());

                        LOG_S(Logger::kLevelAlarm, "play: failure");
                    }
                    break;
                }
            case StepType::head_normal:
                {
                    LOG_S(Logger::kLevelEvent, "data: success");

                    seg_end(buffer_->segment());
                    LOG_S(Logger::kLevelDebug, "data used (" << open_logs_[1].total_elapse << " milliseconds)");

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

        void Live3Demuxer::parse_play(
            Live2PlayInfo & play_info,
            boost::asio::streambuf & buf, 
            error_code & ec)
        {
            if (!ec) {
                std::string buffer = boost::asio::buffer_cast<char const *>(buf.data());
                LOG_S(Logger::kLevelDebug2, "[parse_play] play buffer: " << buffer);

                boost::asio::streambuf buf2;
                util::buffers::buffer_copy(buf2.prepare(buf.size()), buf.data());
                buf2.commit(buf.size());

                util::archive::XmlIArchive<> ia(buf2);
                ia >> play_info;
                if (!ia) 
                {
                    Live2PlayInfoNew playinfo;
                    util::archive::XmlIArchive<> ia1(buf);
                    ia1 >> playinfo;
                    if (!ia1)
                    {
                        ec = bad_file_format;
                    }
                    else
                    {
                        play_info = playinfo;
                    }
                }
            }
        }

        void Live3Demuxer::set_info_by_play(
            Live2PlayInfo & play_info)
        {
            LOG_S(Logger::kLevelDebug, "play succeed (" << open_logs_[0].total_elapse << " milliseconds)");
            //LOG_S(Logger::kLevelDebug, "server host: " << play_info.server_host.to_string());
            //LOG_S(Logger::kLevelDebug, "delay play time: " << play_info.delay_play_time);
            time_t server_time = play_info.dt.st.to_time_t();
            LOG_S(Logger::kLevelDebug, "server time: " << ::ctime(&server_time));
            demux_data().set_server_host(play_info.dt.sh.to_string());
            buffer_->set_play_info(play_info);
        }

        void Live3Demuxer::response(
            error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);

            io_svc_.post(boost::bind(resp, ec));
        }

        bool Live3Demuxer::is_open(
            error_code & ec)
        {
            return demuxer_->is_open(ec);
        }

        error_code Live3Demuxer::cancel(
            error_code & ec)
        {
            play_->cancel();
            return buffer_->cancel(ec);
        }

        error_code Live3Demuxer::pause(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code Live3Demuxer::resume(
            error_code & ec)
        {
            return ec = error::not_support;
        }

        error_code Live3Demuxer::close(
            error_code & ec)
        {
            DemuxerStatistic::close();
            return buffer_->close(ec);
        }

        size_t Live3Demuxer::get_media_count(
            error_code & ec)
        {
            size_t count = demuxer_->get_media_count(ec);
            if (!ec) {
            }
            return count;
        }

        error_code Live3Demuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            error_code & ec)
        {
            demuxer_->get_media_info(index, info, ec);
            if (!ec) {
            }
            return ec;
        }

        boost::uint32_t Live3Demuxer::get_duration(
            error_code & ec)
        {
            ec = not_supported;
            return 0;
        }

        boost::uint32_t Live3Demuxer::get_end_time(
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

        boost::uint32_t Live3Demuxer::get_cur_time(
            error_code & ec)
        {
            return demuxer_->get_cur_time(ec);
        }

        error_code Live3Demuxer::seek(
            boost::uint32_t & time, 
            error_code & ec)
        {
            ec.clear();
            if(0 == time_)
            {
                time_ = time;
            }
            else
            {
                boost::system::error_code ec1;
                buffer_->close_segment(0,ec1);
                buffer_->clear();
                demuxer_->close(ec1);
                if(time > time_)
                {
                    buffer_->set_file_time(time-time_,true);
                }
                else
                {
                    buffer_->set_file_time(time_-time,false);
                }

                demuxer_->open(ec1);
                seg_end(buffer_->segment());
                open_step_ = StepType::finish;
            }
            return ec;
        }

        error_code Live3Demuxer::get_sample(
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

        error_code Live3Demuxer::set_non_block(
            bool non_block, 
            error_code & ec)
        {
            return buffer_->set_non_block(non_block, ec);
        }

        error_code Live3Demuxer::set_time_out(
            boost::uint32_t time_out, 
            error_code & ec)
        {
            return buffer_->set_time_out(time_out, ec);
        }

        error_code Live3Demuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            error_code & ec)
        {
            ec = error_code();
            buffer_->set_http_proxy(addr);
            return ec;
        }

        void Live3Demuxer::seg_beg(
            size_t segment)
        {
            if (segment == 0) {
                open_logs_[1].begin_try();
            }
        }

        void Live3Demuxer::seg_end(
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

// VodDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/vod/VodDemuxer.h"
#include "ppbox/demux/pptv/PptvJump.h"
#include "ppbox/demux/pptv/PptvDrag.h"
#include "ppbox/demux/vod/VodSegments.h"
#include "ppbox/demux/source/HttpOneSegment.h"
#include "ppbox/demux/DemuxerModule.h"
using namespace ppbox::demux::error;

#include "ppbox/ppbox/Common.h"

#include <ppbox/common/Environment.h>

#include <util/protocol/pptv/Url.h>
#include <util/serialization/stl/vector.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h>
#include <util/buffers/BufferCopy.h>

#include <util/archive/TextIArchive.h>
#include <util/archive/TextOArchive.h>

#include <framework/string/Parse.h>
#include <framework/string/StringToken.h>
#include <framework/string/Algorithm.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/network/NetName.h>
using namespace framework::system::logic_error;
using namespace framework::string;
using namespace framework::logger;

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/bind.hpp>
#include <boost/thread/condition_variable.hpp>
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::error;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("VodDemuxer", 0);

namespace ppbox
{
    namespace demux
    {

        VodDemuxer::VodDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint16_t ppap_port, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size)
            : PptvDemuxer(io_svc, buffer_size, prepare_size, segments_ = new VodSegments(io_svc, ppap_port))
            , video_(NULL)
            , jump_(new PptvJump(io_svc, JumpType::vod))
            , drag_(new PptvDrag(io_svc))
            , drag_info_(new VodDragInfoNew())
            , keep_count_(0)
            , can_insert_media_( false )
            , open_step_(StepType::not_open)
            , pending_error_(would_block)
            , msg_queue_( "AdProcesser", ( ( util::daemon::use_module<ppbox::demux::DemuxerModule>(global_daemon()) ) ).shared_memory() )
        {
            set_play_type(PptvDemuxerType::vod);
            segments_->set_vod_demuxer(this);
            segments_->set_buffer_list(buffer_);

            // 计算 keep count, 假设每个影片端的大小至少为15M
            keep_count_ = buffer_size / 15 * 1024 * 1024;
            if (keep_count_ < 2) {
                keep_count_ = 2;
            }
        }

        VodDemuxer::~VodDemuxer()
        {
            if (jump_) {
                delete jump_;
                jump_ = NULL;
            }
            if (video_) {
                delete video_;
                video_ = NULL;
            }
            if (segments_) {
                delete segments_;
                segments_ = NULL;
            }
            if (buffer_) {
                delete buffer_;
                buffer_ = NULL;
            }
        }

        bool VodDemuxer::is_open(
            error_code & ec)
        {
            return is_open(false, ec);
        }

        bool VodDemuxer::is_open(
            bool need_check_seek, 
            boost::system::error_code & ec)
        {
            switch (open_step_) {
            case StepType::finish2:
                {
                    //这里不可能need_check_seek
                    assert(!need_check_seek);
                    ec.clear();
                    break;
                }
            case StepType::finish:
                {
                    if (jump_) {
                        delete jump_;
                        jump_ = NULL;
                    }
                    drag_.reset();
                    drag_info_.reset();
                    //if (!pending_error_)
                        open_step_ = StepType::finish2;
                    //这里不可能need_check_seek
                    assert(!need_check_seek);
                    ec.clear();
                    break;
                }
            //case StepType::drag_normal:
            //    {
            //        ec = pending_error_;
            //        if (drag_info_->is_ready) {
            //            pending_error_ = drag_info_->ec;
            //            open_logs_end(drag_->http_stat(), 2, drag_info_->ec);
            //            LOG_S(Logger::kLevelDebug, "drag used (" << open_logs_[2].total_elapse << " milliseconds)");
            //            if (!pending_error_) {
            //                LOG_S(Logger::kLevelEvent, "drag: success");
            //                process_drag(*drag_info_, pending_error_);
            //            } else {
            //                LOG_S(Logger::kLevelAlarm, "drag: failure");
            //                LOG_S(Logger::kLevelDebug, "drag ec: " << drag_info_->ec.message());
            //            }
            //            if (!pending_error_) {
            //                // 检查延迟的seek操作,seek的错误码不算pending_error
            //                check_pending_seek(ec);
            //                LOG_S(Logger::kLevelDebug, "check_pending_seek ec: " << ec.message());
            //            } else {
            //                ec = pending_error_;
            //            }
            //            open_step_ = StepType::finish;
            //            is_ready_ = true;
            //        }
            //        if (!need_check_seek)
            //            ec.clear();
            //        break;
            //    }
            case StepType::not_open:
                ec = error::not_open;
                break;
            default:
                ec = would_block;
                break;
            }

            return !ec;
        }

        void VodDemuxer::process_insert_media()
        {
            if ( !can_insert_media_ ) return;

            boost::system::error_code ec;
            framework::process::Message msg; 
            msg.level = 0;
            msg.type = 6;

            while ( msg_queue_.pop( msg ) )
            {
                InsertMediaInfo mediainfo;
                boost::asio::streambuf read_buf_;
                std::ostream os(&read_buf_);
                os.write( msg.data.c_str(), msg.data.size() );
                std::istream is(&read_buf_);
                util::archive::TextIArchive<> ia( is );
                ia >> mediainfo;

                HttpOneSegment * source = new HttpOneSegment(
                    io_svc_, DemuxerType::mp4);

                source->set_name( mediainfo.url );
                source->set_segment_size( mediainfo.media_size );
                source->set_segment_time( mediainfo.media_duration );
                //source->set_head_size( mediainfo.head_size );

				LOG_S(Logger::kLevelDebug, "Begin insert media: " << msg.data);
                insert_source( mediainfo.insert_time, source, ec);
                if ( !ec )
                {
                    mediainfo.event_time = (size_t)time(NULL);
                    mediainfo.event_type = INS_TIME;
                    mediainfo.argment = mediainfo.insert_time;

                    framework::process::Message msg;
                    msg.receiver = "AdInserter";
                    msg.level = 0;
                    msg.type = 6;

                    boost::asio::streambuf write_buf;
                    std::ostream os(&write_buf);
                    util::archive::TextOArchive<> oa( os );
                    oa << mediainfo;
                    msg.data = ( char * )boost::asio::detail::buffer_cast_helper( write_buf.data() );

				    LOG_S(Logger::kLevelDebug, "End insert media: " << msg.data);
                    msg_queue_.push( msg );
                }

                msg.sender.clear();
                msg.receiver.clear();
                msg.level = 0;
                msg.type = 6;
                msg.data.clear();
            }
        }

        void VodDemuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            PptvDemuxer::open_beg();
            open_logs_.resize(3);

            LOG_S(Logger::kLevelDebug, "name: " << name);

            segments_->set_name(name);

            resp_ = resp;
            open_step_ = StepType::opening;

            handle_async_open(boost::system::error_code());
        }

        void VodDemuxer::response(
            error_code const & ec)
        {
            open_response_type resp;
            resp.swap(resp_);

            io_svc_.post(boost::bind(resp, ec));
        }

        void VodDemuxer::set_info_by_jump(
            VodJumpInfoNoDrag & jump_info) 
        {
            LOG_S(Logger::kLevelDebug, "user host: " << jump_info.user_host.to_string());
            LOG_S(Logger::kLevelDebug, "server host: " << jump_info.server_host.to_string());
            time_t server_time = jump_info.server_time.to_time_t();
            LOG_S(Logger::kLevelDebug, "server time: " << ::ctime(&server_time));
            demux_data().set_server_host(jump_info.server_host.host_svc());
            segments_->set_server_host_time(
                jump_info.server_host, 
                jump_info.server_time.to_time_t(), 
                jump_info.BWType);
        }

        void VodDemuxer::set_info_by_video(
            VodVideo & video)
        {
            if (!video_) {
                video_ = new VodVideo(video);
            } else {
                *video_ = video;
            }
            LOG_S(Logger::kLevelDebug, "video name: " << video_->name);
            demux_data().set_name(video_->name);
            demux_data().bitrate = video_->bitrate;
        }

        void VodDemuxer::open_logs_end(
            ppbox::common::HttpStatistics const & http_stat, 
            int index, 
            error_code const & ec)
        {
            if (&http_stat != &open_logs_[index])
                open_logs_[index] = http_stat;
            open_logs_[index].total_elapse = open_logs_[index].elapse();
            open_logs_[index].last_last_error = ec;
        }

        void VodDemuxer::handle_async_open(
            error_code const & ecc)
        {
            error_code ec = ecc;
            if (ec) {
                if (ec != boost::asio::error::would_block) {
                    if (open_step_ == StepType::jump) {
                        LOG_S(Logger::kLevelAlarm, "jump: failure");
                        open_logs_end(jump_->http_stat(), 0, ec);
                        LOG_S(Logger::kLevelDebug, "jump failure (" << open_logs_[0].total_elapse << " milliseconds)");
                    } else if (open_step_ == StepType::head_normal 
                        || open_step_ == StepType::head_abnormal) {
                        LOG_S(Logger::kLevelAlarm, "data: failure");
                        open_logs_end(open_logs_[1], 1, ec);
                        LOG_S(Logger::kLevelDebug, "data failure (" << open_logs_[1].total_elapse << " milliseconds)");
                    } else if (open_step_ == StepType::drag_abnormal) {
                        LOG_S(Logger::kLevelAlarm, "drag: failure");
                        open_logs_end(drag_->http_stat(), 2, ec);
                        LOG_S(Logger::kLevelDebug, "drag failure (" << open_logs_[2].total_elapse << " milliseconds)");
                    }

                    DemuxerStatistic::last_error(ec);
                }

                is_ready_ = true;
                response(ec);
                return;
            }

            switch (open_step_) {
            case StepType::opening:
                {
                    if (segments_->get_name().empty()) {
                        ec = empty_name;
                    }

                    if (!ec) {

                        open_logs_[0].reset();

                        open_step_ = StepType::jump;

                        LOG_S(Logger::kLevelEvent, "jump: start");

                        jump_->async_get(
                            segments_->get_jump_url(), 
                            boost::bind(&VodDemuxer::handle_async_open, this, _1));
                        return;
                    } else {
                        LOG_S(Logger::kLevelDebug, "url ec: " << ec.message());
                    }
                    break;
                }
            case StepType::jump:
                {
                    VodJumpInfoNoDrag jump_info;
                    parse_jump(jump_info, jump_->get_buf(), ec);
                    open_logs_end(jump_->http_stat(), 0, ec);

                    LOG_S(Logger::kLevelDebug, "jump used (" << open_logs_[0].total_elapse << " milliseconds)");
                    if (!ec) {
                        LOG_S(Logger::kLevelEvent, "jump: success");

                        if (jump_info.block_size == 0) {
                            set_info_by_jump(jump_info);

                            open_step_ = StepType::drag_abnormal;

                            // 没有drag信息，先取drag，后做data
                            LOG_S(Logger::kLevelDebug, "jump has no drag info");

                            LOG_S(Logger::kLevelEvent, "drag: start");

                            drag_->async_get(
                                segments_->get_drag_url(), 
                                segments_->get_server_host(), 
                                boost::bind(&VodDemuxer::handle_async_open, this, _1));
                            return;
                        } else {
                            set_info_by_jump(jump_info);

                            set_info_by_video(jump_info.video);

                            open_step_ = StepType::head_normal;

                            if (jump_info.video.duration == jump_info.firstseg.duration) {
                                LOG_S(Logger::kLevelEvent, "drag: success");

                                pending_error_.clear();
                                drag_info_->is_ready = true;
                                open_step_ = StepType::head_abnormal;
                            }

                            segments_->add_segment(jump_info.firstseg);

                            if (!ec) {
                                LOG_S(Logger::kLevelDebug, "jump has first drag info");

                                LOG_S(Logger::kLevelEvent, "data: start");

                                BufferDemuxer::async_open("",
                                    boost::bind(&VodDemuxer::handle_async_open, this, _1));

                                return;
                            }
                        }
                    } else {
                        LOG_S(Logger::kLevelDebug, "jump ec: " << ec.message());

                        LOG_S(Logger::kLevelAlarm, "jump: failure");
                    }
                    break;
                }
            case StepType::head_normal:
                {
                    LOG_S(Logger::kLevelEvent, "data: success");

                    // 假造的seg_end
                    seg_end(buffer_->write_segment().segment);
                    LOG_S(Logger::kLevelDebug, "data used (" << open_logs_[1].total_elapse << " milliseconds)");

                    open_step_ = StepType::drag_normal;

                    // 初始化infos
                    error_code lec;
                    boost::uint32_t stream_count = get_media_count(lec);
                    if (!lec) {
                        MediaInfo info;
                        for(boost::uint32_t i = 0; i < stream_count; ++i) {
                            get_media_info(i, info, lec);
                        }
                    }

                    LOG_S(Logger::kLevelEvent, "drag: start");

                    drag_->async_get(
                        segments_->get_drag_url(), 
                        segments_->get_server_host(), 
                        boost::bind(&VodDemuxer::handle_drag, this, drag_info_, get_poster(), _1, _2));
                    break;
                }
            case StepType::drag_abnormal:
                {
                    parse_drag(*drag_info_, drag_->get_buf(), ec);
                    ec = drag_info_->ec;
                    open_logs_end(drag_->http_stat(), 2, ec);
                    LOG_S(Logger::kLevelDebug, "drag used (" << open_logs_[2].total_elapse << " milliseconds)");
                    if (!ec) {
                        LOG_S(Logger::kLevelEvent, "drag: success");

                        process_drag(*drag_info_, ec);
                        if (!ec) {
                            pending_error_.clear();
                            open_step_ = StepType::head_abnormal;

                            LOG_S(Logger::kLevelEvent, "data: start");

                            BufferDemuxer::async_open("", 
                                boost::bind(&VodDemuxer::handle_async_open, this, _1));
                            return;
                        }
                    } else {
                        LOG_S(Logger::kLevelAlarm, "drag ec: " << ec.message());

                        LOG_S(Logger::kLevelAlarm, "drag: failure");
                    }

                    break;
                }
            case StepType::head_abnormal:
                {
                    LOG_S(Logger::kLevelEvent, "data: success");

                    // 假造的seg_end
                    seg_end(buffer_->write_segment().segment);
                    LOG_S(Logger::kLevelDebug, "data used (" << open_logs_[1].total_elapse << " milliseconds)");

                    open_step_ = StepType::finish;

                    break;
                }
            default:
                assert(0);
                return;
            }

            if (ec != boost::asio::error::would_block) {
                DemuxerStatistic::last_error(ec);
            }

            is_ready_ = drag_info_->is_ready;

            response(ec);
        }

        void VodDemuxer::parse_jump(
            VodJumpInfoNoDrag & jump_info,
            boost::asio::streambuf & buf, 
            error_code & ec)
        {
            std::string buffer = boost::asio::buffer_cast<char const *>(buf.data());
            LOG_S(Logger::kLevelDebug2, "[handle_jump] jump buffer: " << buffer);

            boost::asio::streambuf buf2;
            util::buffers::buffer_copy(
                buf2.prepare(buf.size()), 
                buf.data());
            buf2.commit(buf.size());

            util::archive::XmlIArchive<> ia(buf2);
            ia >> (VodJumpInfo &)jump_info;
            if (!ia) {
                util::archive::XmlIArchive<> ia2(buf);
                ia2 >> jump_info;
                if (!ia2) {
                    ec = bad_file_format;
                }
            }
        }

        void VodDemuxer::parse_drag(
            VodDragInfoNew & drag_info, 
            boost::asio::streambuf & buf, 
            error_code const & ec)
        {
            if (ec) {
                drag_info.ec = ec;
                drag_info.is_ready = true;
                return;
            }

            std::string buffer = boost::asio::buffer_cast<char const *>(buf.data());
            LOG_S(Logger::kLevelDebug2, "[parse_drag] drag buffer: " << buffer);

            boost::asio::streambuf buf2;
            util::buffers::buffer_copy(buf2.prepare(buf.size()), buf.data());
            buf2.commit(buf.size());

            util::archive::XmlIArchive<> ia(buf2);
            ia >> drag_info;
            if (!ia) {
                util::archive::XmlIArchive<> ia2(buf);
                VodDragInfo drag_info_old;
                ia2 >> drag_info_old;
                if (!ia2) {
                    drag_info.ec = bad_file_format;
                } else {
                    drag_info = drag_info_old;
                }
            }
        }

        void VodDemuxer::handle_drag(
            boost::shared_ptr<VodDragInfoNew> const & drag_info, 
            post_event_func const & post_event, 
            error_code const & ecc, 
            boost::asio::streambuf & buf)
        {
            parse_drag(*drag_info, buf, ecc);
            // 这里的this可能早已经析构了，不过没关系，这时候process_drag肯定不会被调用
            post_event(boost::bind(&VodDemuxer::process_drag, this, boost::ref(*drag_info), ecc));
        }

        error_code VodDemuxer::cancel(
            error_code & ec)
        {
            jump_->cancel();
            drag_->cancel();
            return buffer_->cancel(ec);
        }

        error_code VodDemuxer::pause(
            error_code & ec)
        {
            DemuxerStatistic::pause();
            return ec = error_code();
        }

        error_code VodDemuxer::resume(
            error_code & ec)
        {
            DemuxerStatistic::resume();
            return ec = error_code();
        }

        error_code VodDemuxer::close(
            error_code & ec)
        {
            if (drag_) {
                drag_->cancel();
                if (open_step_ == StepType::drag_normal)
                    open_logs_end(drag_->http_stat(), 2, boost::asio::error::operation_aborted);
            }
            DemuxerStatistic::close();
            return buffer_->close(ec);
        }

        size_t VodDemuxer::get_media_count(
            error_code & ec)
        {
            if (is_open(ec)) {
                if (jump_) {
                    delete jump_;
                    jump_ = NULL;
                }

                // 假设电影流至少包含音频和视频两个流
                if (media_info_.size() >= 2) {
                    return media_info_.size();
                } else {
                    return BufferDemuxer::get_media_count(ec);
                }
            }
            return 0;
        }

        error_code VodDemuxer::get_media_info(
            size_t index, 
            MediaInfo & info, 
            error_code & ec)
        {
            if (is_open(ec)) {
                if (jump_) {
                    delete jump_;
                    jump_ = NULL;
                }

                if (media_info_.size() >= 2) {
                    info = media_info_[index];
                } else {
                    BufferDemuxer::get_media_info(index, info, ec);
                    if (!ec && media_info_.size() == index) {
                        media_info_.push_back(info);
                    }
                }
            }
            return ec;
        }

        boost::uint32_t VodDemuxer::get_duration(
            error_code & ec)
        {
            if (is_open(ec)) {
                if (jump_) {
                    delete jump_;
                    jump_ = NULL;
                }

                return video_->duration;
            }
            return 0;
        }

        error_code VodDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            error_code & ec)
        {
            segments_->set_http_proxy(addr);
            return ec = error_code();
        }

        void VodDemuxer::process_drag(
            VodDragInfoNew & drag_info, 
            error_code & ec)
        {
            ec.clear();

            set_info_by_video(drag_info.video);

            std::vector<VodSegmentNew> & segments = drag_info.segments;
            for (size_t i = 0; !ec && i < segments.size(); ++i) {
                if (i == 0 && segments_->segments_info().size() > 0)
                    continue;
                segments_->add_segment(segments[i]);
            }

            //HttpOneSegment * source = new HttpOneSegment(io_svc_, DemuxerType::mp4);
            //source->set_name("http://192.168.45.211/movies/yu_2.mp4");
            //source->set_segment_size(1192113);
            //source->set_segment_time(16580);
            //source->set_head_size(9462);
            //error_code ecc = error_code();
            //boost::uint32_t time = 20000;
            //insert_source( time, source, ecc );

            can_insert_media_ = true;
            process_insert_media();
        }

        void VodDemuxer::seg_beg(
            size_t segment)
        {
            if (segment < segments_->segments_info().size()) {
                if (segment == 0) {
                    open_logs_[1].begin_try();
                }
                if (segments_->segments_info()[segment].va_rid.size() >= 32) {
                    demux_data().set_rid(segments_->segments_info()[segment].va_rid);
                }
            }
        }

        void VodDemuxer::seg_end(
            size_t segment)
        {
            if (segment < segments_->segments_info().size()) {
                if (segment == 0) {
                    open_logs_[1].end_try(segments_->http_stat());
                    if (open_logs_[1].try_times == 1)
                        open_logs_[1].response_data_time = open_logs_[1].total_elapse;
                }
            }
        }

        error_code VodDemuxer::check_pending_seek(
            error_code & ec)
        {
            /*if (seek_time_ > 1) {
                boost::uint32_t seek_time = seek_time_;
                seek_time_ = boost::uint32_t(-1);
                seek(seek_time, ec);
            } else {
                ec.clear();
            }*/
            return ec;
        }

    } // namespace demux
} // namespace ppbox

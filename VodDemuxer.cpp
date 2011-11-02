// VodDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/VodDemuxer.h"
#include "ppbox/demux/PptvJump.h"
#include "ppbox/demux/PptvDrag.h"
#include "ppbox/demux/VodSegments.h"
#include "ppbox/demux/mp4/Mp4BufferDemuxer.h"
using namespace ppbox::demux::error;

#include <ppbox/common/Environment.h>

#include <util/protocol/pptv/Url.h>
#include <util/serialization/stl/vector.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h>

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

        class VodSegmentDemuxer
            : public Mp4BufferDemuxer<VodSegments>
            , public VodSegmentNew
        {
        public:
            VodSegmentDemuxer(
                VodSegmentNew const & segment, 
                VodSegments & buf, 
                error_code & ec)
                : Mp4BufferDemuxer<VodSegments>(buf)
                , VodSegmentNew(segment)
            {
                open(segment.head_length, segment.file_length, ec);
            }
        };

        VodDemuxer::VodDemuxer(
            boost::asio::io_service & io_svc, 
            boost::uint16_t ppap_port, 
            boost::uint32_t buffer_size, 
            boost::uint32_t prepare_size)
            : Demuxer(io_svc, (buffer_ = new VodSegments(io_svc, ppap_port, buffer_size, prepare_size))->buffer_stat())
            , video_(NULL)
            , jump_(new PptvJump(io_svc, JumpType::vod))
            , drag_(new PptvDrag(io_svc))
            , drag_info_(new VodDragInfoNew())
            , seek_time_(0)
            , keep_count_(0)
            , open_step_(StepType::not_open)
            , pending_error_(would_block)
        {
            set_play_type(DemuxerType::vod);
            buffer_->set_vod_demuxer(this);

            // 计算 keep count, 假设每个影片端的大小至少为15M
            keep_count_ = buffer_size / 15 * 1024 * 1024;
            if (keep_count_ < 2) {
                keep_count_ = 2;
            }
        }

        VodDemuxer::~VodDemuxer()
        {
            for (size_t i = 0; i < segments_.size(); ++i) {
                delete segments_[i];
            }
            if (jump_) {
                delete jump_;
                jump_ = NULL;
            }
            if (video_) {
                delete video_;
                video_ = NULL;
            }
            delete buffer_;
        }

        bool VodDemuxer::is_open(
            error_code & ec)
        {
            return is_open(false, ec);;
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
            case StepType::drag_normal:
                {
                    ec = pending_error_;
                    if (drag_info_->is_ready) {
                        pending_error_ = drag_info_->ec;
                        open_logs_end(drag_->http_stat(), 2, drag_info_->ec);
                        LOG_S(Logger::kLevelDebug, "drag used (" << open_logs_[2].total_elapse << " milliseconds)");
                        if (!pending_error_) {
                            LOG_S(Logger::kLevelEvent, "drag: success");
                            process_drag(*drag_info_, pending_error_);
                        } else {
                            LOG_S(Logger::kLevelAlarm, "drag: failure");
                            LOG_S(Logger::kLevelDebug, "drag ec: " << drag_info_->ec.message());
                        }
                        if (!pending_error_) {
                            // 检查延迟的seek操作,seek的错误码不算pending_error
                            check_pending_seek(ec);
                            LOG_S(Logger::kLevelDebug, "check_pending_seek ec: " << ec.message());
                        } else {
                            ec = pending_error_;
                        }
                        open_step_ = StepType::finish;
                        is_ready_ = true;
                    }
                    if (!need_check_seek)
                        ec.clear();
                    break;
                }
            case StepType::not_open:
                ec = error::not_open;
                break;
            default:
                ec = would_block;
                break;
            }

            return !ec;
        }

        void VodDemuxer::async_open(
            std::string const & name, 
            open_response_type const & resp)
        {
            name_ = name;
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
            buffer_->set_server_host_time(
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

                    open_logs_.resize(3);

                    std::string::size_type slash = name_.find('|');
                    std::string url;
                    if (slash == std::string::npos) {
                        ec = empty_name;
                    } else {
                        std::string key = name_.substr(0, slash);
                        url = name_.substr(slash + 1);
                        if (url.size() > 4 && url.substr(url.size() - 4) == ".mp4") {
                            if (url.find('%') == std::string::npos) {
                                url = Url::encode(url);
                            }
                        } else {
                            url = pptv::url_decode(url, key);
                            StringToken st(url, "||");
                            if (!st.next_token(ec)) {
                                url = st.remain();
                            }
                        }
                    }

                    if (url.empty()) {
                        ec = empty_name;
                    }

                    if (!ec) {
                        LOG_S(Logger::kLevelDebug, "url: " << url);

                        buffer_->set_name(url);
                        open_logs_[0].reset();

                        open_step_ = StepType::jump;

                        LOG_S(Logger::kLevelEvent, "jump: start");

                        jump_->async_get(
                            buffer_->get_jump_url(), 
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
                                buffer_->get_drag_url(), 
                                buffer_->get_server_host(), 
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

                            buffer_->set_segments(std::vector<VodSegmentNew>(1, jump_info.firstseg));
                            segments_.push_back(new VodSegmentDemuxer(jump_info.firstseg, *buffer_, ec));

                            if (!ec) {
                                LOG_S(Logger::kLevelDebug, "jump has first drag info");

                                LOG_S(Logger::kLevelEvent, "data: start");

                                segments_[0]->async_open(
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
                    seg_end(buffer_->segment());
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
                        buffer_->get_drag_url(), 
                        buffer_->get_server_host(), 
                        boost::bind(&VodDemuxer::handle_drag, drag_info_, _1, _2));
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

                            segments_[0]->async_open(
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

            drag_info.is_ready = true;
        }

        void VodDemuxer::handle_drag(
            boost::shared_ptr<VodDragInfoNew> const & drag_info, 
            error_code const & ecc, 
            boost::asio::streambuf & buf)
        {
            parse_drag(*drag_info, buf, ecc);
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
                    return segments_[buffer_->read_segment()]->get_media_count(ec);
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
                    segments_[buffer_->read_segment()]->get_media_info(index, info, ec);
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

        boost::uint32_t VodDemuxer::get_end_time(
            error_code & ec, 
            error_code & ec_buf)
        {
            tick_on();
            if ((ec = extern_error_)) {
                return 0;
            } else if (is_open(seek_time_, ec)) {
                size_t seg_write = buffer_->write_segment();
                if (seg_write < segments_.size()) {
                    boost::uint32_t buffer_time = 
                        segments_[seg_write]->get_end_time(ec, ec_buf);
                    return segments_[seg_write]->duration_offset + buffer_time;
                } else {
                    // 下载结束
                    ec.clear();
                    if (!(ec_buf = pending_error_)) {
                        ec_buf = boost::asio::error::eof;
                    }
                    return segments_.back()->duration_offset + segments_.back()->duration;
                }
            } else if (ec == would_block) {
                ec_buf = ec;
                return segments_.back()->duration_offset + segments_.back()->duration;
            } else {
                ec_buf = ec;
                return 0;
            }
        }

        boost::uint32_t VodDemuxer::get_cur_time(
            error_code & ec)
        {
            if (is_open(seek_time_, ec)) {
                size_t seg_read = buffer_->read_segment();
                if (seg_read < segments_.size()) {
                    return segments_[seg_read]->duration_offset + segments_[seg_read]->get_cur_time(ec);
                } else {
                    ec.clear();
                    return segments_.back()->duration_offset + segments_.back()->duration;
                }
            } else if (ec == would_block) {
                return segments_.back()->duration_offset + segments_.back()->duration;
            } else {
                return 0;
            }
        }

        error_code VodDemuxer::seek(
            boost::uint32_t & time, 
            error_code & ec)
        {
            boost::uint32_t seek_time = seek_time_;
            seek_time_ = 0; // discard old seek
            if (seek_time == (boost::uint32_t)-1 // do not call is_open when called by is_open
                || is_open(ec)) {
                if (time < video_->duration) {
                    for (size_t i = 0; i < segments_.size(); ++i) {
                        if (time < segments_[i]->duration_offset + segments_[i]->duration) {
                            demux_data().segment = i;
                            time -= segments_[i]->duration_offset;
                            segments_[i]->seek(time, ec);
                            time += segments_[i]->duration_offset;
                            // seek_time_ == -1说明是get_drag里面调用的seek
                            if (seek_time != (boost::uint32_t)-1) {
                                DemuxerStatistic::seek(time, ec);
                            }

                            // 设置timescal对应的offset
                            if (!ec || ec == boost::asio::error::would_block) {
                                for(boost::uint32_t index = 0; index < media_info_.size(); ++index) {
                                    media_info_[index].duration = 
                                        segments_[i]->duration_offset_us * media_info_[index].time_scale / 1000000;
                                }
                            }

                            return ec;
                        }
                    }

                    if (pending_error_) {
                        ec = pending_error_;
                        if (ec == would_block) {
                            seek_time_ = time;
                            DemuxerStatistic::seek(time, ec);
                        }
                    } else {
                        ec = bad_file_format;
                    }
                } else {
                    ec = out_of_range;
                }
            }
            return ec;
        }

        size_t VodDemuxer::get_segment_count(
            boost::system::error_code & ec)
        {
            if (is_open(ec))
                return segments_.size();
            return 0;
        }

        error_code VodDemuxer::get_segment_info(
            SegmentInfo & info, 
            bool need_head_data, 
            error_code & ec)
        {
            if (is_open(ec)) {
                if (info.index == size_t(-1)) {
                    info.index = buffer_->read_segment();
                }
                if (info.index < segments_.size()) {
                    VodSegmentDemuxer * demuxer = segments_[info.index];
                    info.duration = demuxer->duration;
                    info.duration_offset = demuxer->duration_offset;
                    info.file_length = demuxer->file_length;
                    info.head_length = demuxer->head_length;
                    ec = error_code();
                    if (need_head_data)
                        demuxer->get_head_data(info.head_data, ec);
                } else {
                    ec = out_of_range;
                }
            }
            return ec;
        }

        error_code VodDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            tick_on();
            //framework::timer::TimeCounter tc;
            //if (tc.elapse() > 10) {
            //    LOG_S(Logger::kLevelDebug, "[get_sample] elapse1: " << tc.elapse());
            //}
            if ((ec = extern_error_)) {
            } else if (is_open(seek_time_, ec)) {
                size_t seg_read = buffer_->read_segment();
                assert(seg_read < segments_.size());
                if (seg_read < segments_.size()) {
                    segments_[seg_read]->get_sample(sample, ec);
                    if (ec == no_more_sample) {
                        if (++seg_read < segments_.size()) {
                            LOG_S(Logger::kLevelDebug, 
                                "segment: " << seg_read << 
                                " duration: " << segments_[seg_read]->duration);
                            demux_data().segment = seg_read;
                            boost::uint32_t seek_time = 0;
                            framework::timer::TimeCounter tc;
                            segments_[seg_read]->seek(seek_time, ec) 
                                || segments_[seg_read]->get_sample(sample, ec);
                            if (tc.elapse() > 10) {
                                LOG_S(framework::logger::Logger::kLevelDebug, 
                                    "[get_sample] get_sample: " << tc.elapse());
                            }
                            // 设置timescal对应的offset
                            for(boost::uint32_t i = 0; i < media_info_.size(); ++i) {
                                media_info_[i].duration = 
                                    segments_[seg_read]->duration_offset_us * media_info_[i].time_scale / 1000000;
                            }

                            // 清楚前后mp4头占用的内存
                            release_head_buffer(2, seg_read, keep_count_);
                            if (tc.elapse() > 10) {
                                LOG_S(framework::logger::Logger::kLevelDebug, 
                                    "[get_sample] release_head_buffer: " << tc.elapse());
                            }
                        } else {
                            if (pending_error_) {
                                ec = pending_error_;
                            }
                        }
                    }
                    if (!ec) {
                        sample.time += segments_[seg_read]->duration_offset;
                        sample.ustime += segments_[seg_read]->duration_offset_us;

                        if (sample.itrack != boost::uint32_t(-1)) {
                            sample.pts += media_info_[sample.itrack].duration;
                            sample.dts += media_info_[sample.itrack].duration;
                        }

                        if (sample.itrack != sample.itrack) {
                            sample.pts += media_info_[sample.itrack].duration;
                            sample.dts += media_info_[sample.itrack].duration;
                        }
                        play_on(sample.time);
                    }
                } else {
                    if (pending_error_) {
                        ec = pending_error_;
                    } else {
                        ec = out_of_range;
                    }
                }
            }
            if (ec == would_block) {
                block_on();
            }
            //if (tc.elapse() > 20) {
            //    LOG_S(Logger::kLevelDebug, "[get_sample] elapse2: " << tc.elapse());
            //}
            return ec;
        }

        error_code VodDemuxer::set_non_block(
            bool non_block, 
            error_code & ec)
        {
            return buffer_->set_non_block(non_block, ec);
        }

        error_code VodDemuxer::set_time_out(
            boost::uint32_t time_out, 
            error_code & ec)
        {
            return buffer_->set_time_out(time_out, ec);
        }

        error_code VodDemuxer::set_http_proxy(
            framework::network::NetName const & addr, 
            error_code & ec)
        {
            buffer_->set_http_proxy(addr);
            return ec = error_code();
        }

        void VodDemuxer::process_drag(
            VodDragInfoNew & drag_info, 
            error_code & ec)
        {
            ec.clear();

            set_info_by_video(drag_info.video);

            std::vector<VodSegmentNew> & segments = drag_info.segments;
            buffer_->set_segments(segments);
            for (size_t i = 0; !ec && i < segments.size(); ++i) {
                if (i == 0 && segments_.size() > 0)
                    continue;

                segments_.push_back(new VodSegmentDemuxer(segments[i], *buffer_, ec));
            }
        }

        void VodDemuxer::seg_beg(
            size_t segment)
        {
            if (segment < segments_.size()) {
                if (segment == 0) {
                    open_logs_[1].begin_try();
                }
                if (segments_[segment]->va_rid.size() >= 32) {
                    demux_data().set_rid(segments_[segment]->va_rid);
                }
            }
        }

        void VodDemuxer::seg_end(
            size_t segment)
        {
            if (segment < segments_.size()) {
                if (segment == 0) {
                    open_logs_[1].end_try(buffer_->http_stat());
                    if (open_logs_[1].try_times == 1)
                        open_logs_[1].response_data_time = open_logs_[1].total_elapse;
                }
            }
        }

        error_code VodDemuxer::check_pending_seek(
            error_code & ec)
        {
            if (seek_time_ > 1) {
                boost::uint32_t seek_time = seek_time_;
                seek_time_ = boost::uint32_t(-1);
                seek(seek_time, ec);
            } else {
                ec.clear();
            }
            return ec;
        }

        void VodDemuxer::release_head_buffer(
            boost::uint32_t before,
            boost::uint32_t cur,
            boost::uint32_t after)
        {
            boost::int32_t before_end = cur - before;
            if (before_end > 0) {
                for (boost::int32_t i = 0; i < before_end; ++i) {
                    segments_[i]->release();
                    //LOG_S(Logger::kLevelAlarm, "before cur: " << cur << "release segment: " << i << " head");
                }
            }

            boost::int32_t after_befin = cur+after+1;
            if (after_befin < (boost::int32_t)segments_.size()) {
                for (size_t i = after_befin; i < segments_.size(); ++i) {
                    segments_[i]->release();
                    //LOG_S(Logger::kLevelAlarm, "after cur: " << cur << "release segment: " << i << " head");
                }
            }
        }

    } // namespace demux
} // namespace ppbox

// VodSource.h

#include "ppbox/demux/Common.h"
#include "ppbox/demux/vod/VodSource.h"
#include "ppbox/demux/pptv/PptvJump.h"
#include "ppbox/demux/pptv/PptvDrag.h"
#include "ppbox/demux/base/DemuxerError.h"

#include <util/protocol/pptv/Url.h>
#include <util/protocol/pptv/TimeKey.h>
#include <util/archive/XmlIArchive.h>
#include <util/archive/ArchiveBuffer.h>
#include <util/buffers/BufferCopy.h>

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

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("VodSource", 0);

#ifndef PPBOX_DNS_VOD_JUMP
#define PPBOX_DNS_VOD_JUMP "(tcp)(v4)jump.150hi.com:80"
#endif

#ifndef PPBOX_DNS_VOD_DRAG
#define PPBOX_DNS_VOD_DRAG "(tcp)(v4)drag.150hi.com:80"
#endif

namespace ppbox
{
    namespace demux
    {
        static const framework::network::NetName dns_vod_jump_server(PPBOX_DNS_VOD_JUMP);
        static const framework::network::NetName dns_vod_drag_server(PPBOX_DNS_VOD_DRAG);

        static inline std::string addr_host(
            framework::network::NetName const & addr)
        {
            return addr.host() + ":" + addr.svc();
        }

        VodSource::VodSource(
            boost::asio::io_service & io_svc)
            : HttpSource(io_svc)
            , video_(NULL)
            , jump_(new PptvJump(io_svc, JumpType::vod))
            , drag_(new PptvDrag(io_svc))
            , open_step_(StepType::not_open)
            , bwtype_(0)
            , vod_port_(0)
            , url_("http://localhost/")
            , max_dl_speed_(boost::uint32_t(-1))
            , first_seg_(true)
            , know_seg_count_(false)
        {
        }

        VodSource::~VodSource()
        {
            if (jump_) {
                delete jump_;
                jump_ = NULL;
            }
            if (video_) {
                delete video_;
                video_ = NULL;
            }
        }

        void VodSource::async_open(SourceBase::response_type const &resp)
        {
            resp_ = resp;
            open_step_ = StepType::opening;
            handle_async_open(error_code());
        }

        error_code VodSource::cancel(
            boost::system::error_code & ec)
        {
            assert(NULL != jump_);
            assert(NULL != drag_);
            jump_->cancel();
            drag_->cancel();
            ec.clear();
            return ec;
        }

        error_code VodSource::close(
            error_code & ec)
        {
            if (drag_) {
                drag_->cancel();
            }
            ec.clear();
            return ec;
        }

        framework::string::Url VodSource::get_jump_url() const
        {
            framework::string::Url url("http://localhost/");
            url.host(dns_vod_jump_server.host());
            url.svc(dns_vod_jump_server.svc());
            url.path("/" + name_ + "dt");
            return url;
        }
        framework::string::Url VodSource::get_drag_url() const
        {
            framework::string::Url url("http://localhost/");
            url.host(dns_vod_drag_server.host());
            url.svc(dns_vod_drag_server.svc());
            url.path("/" + name_ + "0drag");
            url.param("type", "ppbox");
            return url;
        }

        void VodSource::parse_drag(
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
                    drag_info.ec = error::bad_file_format;
                } else {
                    drag_info = drag_info_old;
                }
            }
        }

        void VodSource::parse_jump(
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
                    ec = error::bad_file_format;
                }
            }
        }

        std::string VodSource::get_key() const
        {
            return util::protocol::pptv::gen_key_from_time(server_time_ + (Time::now() - local_time_).total_seconds());
        }

        void VodSource::set_info_by_jump(
            VodJumpInfoNoDrag & jump_info)
        {
            set_info_by_video(jump_info.video);
            server_host_ = jump_info.server_host;
            bwtype_ = jump_info.BWType;
            server_time_ = jump_info.server_time.to_time_t();
            local_time_ = Time::now();

            assert(segments_.size() == 0);
            if (jump_info.block_size != 0) {
                segments_.push_back(jump_info.firstseg);
            }
        }

        void VodSource::set_info_by_drag(
            VodDragInfoNew & drag_info)
        {
            set_info_by_video(drag_info.video);

            std::vector<VodSegmentNew> & segmentsTmp = drag_info.segments;
            while (segments_.size() > 1)
            {
                segments_.erase(segments_.end());
            }

            for (size_t i = 0;  i < segmentsTmp.size(); ++i) 
            {
                if (i == 0 && segments_.size() > 0)
                    continue;
                    segments_.push_back(segmentsTmp[i]);
            }

            know_seg_count_ = true;
        }

        void VodSource::set_info_by_video(
            VodVideo & video)
        {
            if (NULL == video_) {
                video_ = new VodVideo(video);
            } else {
                *video_ = video;
            }
        }


        void VodSource::handle_async_open(
            error_code const & ecc)
        {
            error_code ec = ecc;
            if (ec) 
            {
                if (ec != boost::asio::error::would_block) 
                {
                    if (open_step_ == StepType::jump) 
                    {
                        LOG_S(Logger::kLevelAlarm, "jump: failure");
                        response(ec);
                    } 
                    else if (open_step_ == StepType::drag) 
                    {
                        LOG_S(Logger::kLevelAlarm, "data: failure");
                    }
                }
                return;
            }

            switch (open_step_) 
            {
            case StepType::opening:
                {

                    if (!ec) 
                    {
                        open_step_ = StepType::jump;

                        LOG_S(Logger::kLevelEvent, "jump: start");

                        jump_->async_get(
                            get_jump_url(), 
                            boost::bind(&VodSource::handle_async_open, this, _1));
                        return;
                    }
                    else 
                    {
                        LOG_S(Logger::kLevelDebug, "url ec: " << ec.message());
                    }
                    break;
                }
            case StepType::jump:
                {
                    VodJumpInfoNoDrag jump_info;
                    parse_jump(jump_info, jump_->get_buf(), ec);
                    if (ec)
                    {
                        break;
                    }

                    set_info_by_jump(jump_info);

                    if (segments_.size() > 0 && jump_info.video.duration == jump_info.firstseg.duration)
                    {
                        know_seg_count_ = true;
                        open_step_ = StepType::finish;
                    }
                    else
                    {
                        open_step_ = StepType::drag;
                        drag_->async_get(
                            get_drag_url(), 
                            jump_info.server_host, 
                            boost::bind(&VodSource::handle_async_open, this, _1));
                        return;
                    }
                    break;
                }
            case StepType::drag:
                {
                    open_step_ = StepType::finish;
                    VodDragInfoNew drag_info;
                    parse_drag(drag_info,drag_->get_buf(),ec);
                    if (!ec) {
                        set_info_by_drag(drag_info);
                    }
                    break;
                }
            default:
                assert(0);
                return;
            }

            response(ec);
        }

        void VodSource::response(
            error_code const & ec)
        {
            SourceBase::response_type resp;
            resp.swap(resp_);
            ios_service().post(boost::bind(resp, ec));
        }

        bool VodSource::is_open()
        {
            bool ret = false;
            switch (open_step_) {
                case StepType::finish:
                {
                    if (jump_) {
                        delete jump_;
                        jump_ = NULL;
                    }
                    drag_.reset();
                    open_step_ = StepType::finish2;
                    ret = true;
                }
                break;
                case StepType::finish2:
                case StepType::drag:
                {
                    ret = true;
                }
                break;
                default:
                {
                }
                break;
            }
            return ret;
        }

        error_code VodSource::time_seek(
            boost::uint64_t time, 
            SegmentPositionEx & abs_position, 
            SegmentPositionEx & position, 
            error_code & ec)
        {
            SourceBase::time_seek(time, abs_position, position, ec);
            if (position.total_state == SegmentPositionEx::not_exist 
                && know_seg_count_
                && !ec) {
                    ec = framework::system::logic_error::out_of_range;
            }
            return ec;
        }

        void VodSource::update_segment(size_t segment)
        {
            if (segments_.size() == segment ) {
                VodSegmentNew newSegment;
                newSegment.duration = boost::uint32_t(-1);
                newSegment.file_length = boost::uint64_t(-1);
                newSegment.head_length = boost::uint64_t(-1);
                segments_.push_back(newSegment);
            } else if (segments_.size() > segment) {
            } else {
                assert(false);
            }
        }

        error_code VodSource::segment_open(
            size_t segment, 
            boost::uint64_t beg, 
            boost::uint64_t end, 
            error_code & ec)
        {
            update_segment(segment);
            return HttpSource::segment_open(segment,beg,end,ec);
        }

        void VodSource::segment_async_open(
            size_t segment, 
            boost::uint64_t beg, 
            boost::uint64_t end, 
            SourceBase::response_type const & resp) 
        {
            update_segment(segment);
            HttpSource::segment_async_open(segment,beg,end,resp);
        }

        error_code VodSource::get_request(
            size_t segment, 
            boost::uint64_t & beg, 
            boost::uint64_t & end, 
            framework::network::NetName & addr, 
            util::protocol::HttpRequest & request, 
            error_code & ec)
        {
            util::protocol::HttpRequestHead & head = request.head();
            ec = error_code();
            if (segment < segments_.size()) {
                if (vod_port_ 
                    && buffer()->num_try() > 3) {
                        buffer()->set_total_req(1);
                        first_seg_ = true;
                        vod_port_ = 0;
                }
                if (first_seg_) {
                    first_seg_ = false;
                    url_.host(server_host_.host());
                    url_.svc(server_host_.svc());
                    if (vod_port_) {
                        HttpSource::set_time_out(0, ec);
                        addr.host("127.0.0.1");
                        addr.port(vod_port_);
                        head.host.reset(addr_host(addr));
                    } else {
                        HttpSource::set_time_out(5 * 1000, ec);
                        if (proxy_addr_.host().empty()) {
                            addr = server_host_;
                        } else {
                            addr = proxy_addr_;
                        }
                        head.host.reset(addr_host(server_host_));
                        if (head.pragma.empty())
                            head.pragma.push_back("Client=PPLiveVA/1,5,2,1");
                    }
                }
                url_.path("/" + format(segment) + "/" + name_);
                url_.param("key", get_key());
                if (vod_port_) {
                    Url url("http://localhost/ppvaplaybyopen?");
                    url.param("url", url_.to_string());
                    url.param("rid", segments_[segment].va_rid);
                    url.param("blocksize", format(segments_[segment].block_size));
                    url.param("filelength", format(segments_[segment].file_length));
                    url.param("headlength", format(segments_[segment].head_length));
                    url.param("autoclose", "false");
                    boost::uint32_t buf_time = vod_demuxer_->buf_time();
                    url.param("drag", buf_time < 15000 ? "1" : "0");
                    url.param("headonly", end <= segments_[segment].head_length ? "1" : "0");
                    url.param("BWType", format(bwtype_));

                    if(max_dl_speed_ != boost::uint32_t(-1))
                        url.param("speedlimit", format(max_dl_speed_));
                    url.param("blocknum", format(segments_[segment].block_num));

                    url.encode();
                    head.path = url.path_all();
                } else {
                    head.path = url_.path_all();
                }

                LOG_S(framework::logger::Logger::kLevelDebug, "Segment url: " << url_.to_string());
            } else {
                ec = item_not_exist;
            }
            return ec;
        }

        DemuxerType::Enum VodSource::demuxer_type() const
        {
            return DemuxerType::mp4;
        }

        void VodSource::set_url(std::string const &url)
        {
            error_code ec;
            std::string::size_type slash = url.find('|');
            if (slash == std::string::npos) {
                return;
            } 
            std::string key = url.substr(0, slash);
            std::string playlink = url.substr(slash + 1);

            playlink = framework::string::Url::decode(playlink);
            framework::string::Url request_url(playlink);
            playlink = request_url.path().substr(1);
            std::string strBwtype = request_url.param("bwtype");

            if(!strBwtype.empty()) {
                bwtype_ = framework::string::parse<boost::int32_t>(strBwtype);
            }

            if (playlink.size() > 4 && playlink.substr(playlink.size() - 4) == ".mp4") {
                if (playlink.find('%') == std::string::npos) {
                    playlink = Url::encode(playlink, ".");
                }
            } else {
                playlink = util::protocol::pptv::url_decode(playlink, key);
                StringToken st(playlink, "||");
                if (!st.next_token(ec)) {
                    playlink = st.remain();
                }
            }
            name_ = playlink;
        }

        error_code VodSource::reset(
            SegmentPositionEx & segment)
        {
            assert(video_);
            segment.segment = 0;
            segment.source = this;
            segment.shard_beg = segment.size_beg = 0;
            boost::uint64_t seg_size = segment_size(segment.segment);
            segment.shard_end = segment.size_end = (seg_size == boost::uint64_t(-1) ? boost::uint64_t(-1): segment.size_beg + seg_size);
            segment.time_beg = segment.time_beg = 0;
            boost::uint64_t seg_time = segment_time(segment.segment);
            segment.time_end = (seg_time == boost::uint64_t(-1) ? boost::uint64_t(-1): segment.time_beg + seg_time );
            if (segment.shard_end == boost::uint64_t(-1)) {
                segment.total_state = SegmentPositionEx::not_exist;
            } else {
                segment.total_state = SegmentPositionEx::is_valid;
            }
            if (segment.time_end == boost::uint64_t(-1)) {
                segment.time_state = SegmentPositionEx::not_exist;
            } else {
                segment.time_state = SegmentPositionEx::is_valid;
            }
            begin_segment_ = segment;
            return error_code();
        }

        error_code VodSource::get_duration(
            DurationInfo & info,
            error_code & ec)
        {
            ec.clear();
            if (NULL != video_) {
                info.total = video_->duration;
                info.begin = 0;
                info.end = video_->duration;
                info.redundancy = 0;
                info.interval = 0;
            } else {
                ec = error::not_open;
            }
            return ec;
        }

        size_t VodSource::segment_count() const
        {
            size_t ret = size_t(-1);
            if (know_seg_count_) {
                ret = segments_.size();
            }
            return ret;
        }

        boost::uint64_t VodSource::segment_size(size_t segment)
        {
            boost::uint64_t ret = boost::uint64_t(-1);
            if (segments_.size() > segment ) {
                ret = segments_[segment].file_length;
            }
            return ret;
        }

        boost::uint64_t VodSource::segment_time(size_t segment)
        {
            boost::uint64_t ret = boost::uint64_t(-1);
            if (segments_.size() > segment ) {
                ret = segments_[segment].duration;
            }
            return ret;
        }

        void VodSource::update_segment_duration(size_t segment, boost::uint32_t time)
        {
            if (segments_.size() > segment ) {
                segments_[segment].duration = time;
            }
        }

        void VodSource::update_segment_file_size(size_t segment, boost::uint64_t fsize)
        {
            if (segments_.size() > segment) {
                if (!know_seg_count_ 
                    && segments_[segment].file_length == boost::uint64_t(-1)) {
                    segments_[segment].file_length = fsize;
                }
            }

            segment++;
            if (!know_seg_count_ && segments_.size() == segment) {
                // add a segment
                VodSegmentNew vod_seg;
                vod_seg.duration = boost::uint32_t(-1);
                vod_seg.file_length = boost::uint64_t(-1);
                vod_seg.head_length = boost::uint64_t(-1);
                segments_.push_back(vod_seg);
            }
        }

        void VodSource::update_segment_head_size(size_t segment, boost::uint64_t hsize)
        {
        }

    } // namespace demux
} // namespace ppbox

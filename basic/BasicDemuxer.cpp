// BasicDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/BasicDemuxer.h"
#include "ppbox/demux/basic/JointContext.h"

#include <ppbox/data/base/MediaInfo.h>
#include <ppbox/data/base/StreamStatus.h>

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/bind.hpp>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.BasicDemuxer", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        BasicDemuxer::BasicDemuxer(
            boost::asio::io_service & io_svc, 
            streambuffer_t & buf)
            : Demuxer(io_svc)
            , buf_(buf)
            , is_open_(false)
            , joint_(NULL)
            , timestamp_(NULL)
            , timestamp2_(NULL)
        {
        }

        BasicDemuxer::~BasicDemuxer()
        {
        }

        boost::system::error_code BasicDemuxer::get_media_info(
            MediaInfo & info, 
            boost::system::error_code & ec) const
        {
            info.duration = get_duration(ec);
            return ec;
        }

        bool BasicDemuxer::get_stream_status(
            StreamStatus & info, 
            boost::system::error_code & ec)
        {
            using ppbox::data::invalid_size;

            assert(joint_ == NULL);

            info.byte_range.beg = 0;
            info.byte_range.end = invalid_size;
            info.byte_range.pos = buf_.pubseekoff(0, std::ios::cur, std::ios::in);
            info.byte_range.buf = buf_.pubseekoff(0, std::ios::end, std::ios::in);
            buf_.pubseekoff(info.byte_range.pos, std::ios::beg, std::ios::in);

            info.time_range.beg = 0;
            info.time_range.end = get_duration(ec);
            info.time_range.pos = get_cur_time(ec);
            info.time_range.buf = get_end_time(ec);

            if (info.time_range.buf < info.time_range.pos) {
                info.time_range.buf = info.time_range.pos;
            }

            return !ec;
        }

        boost::system::error_code BasicDemuxer::reset(
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            size_t n = get_stream_count(ec);
            if (n == 0)
                return ec;
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                get_stream_info(i, info, ec);
                dts.push_back(info.start_time);
            }
            boost::uint64_t delta  = 0;
            boost::uint64_t offset = seek(dts, delta, ec);
            if (ec) {
                return ec;
            }
            if (buf_.pubseekpos(offset) != std::streampos(offset)) {
                ec = error::file_stream_error;
                return ec;
            }
            return ec;
        }

        boost::system::error_code BasicDemuxer::seek(
            boost::uint64_t & time, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            boost::uint64_t delta  = 0;
            std::vector<boost::uint64_t> dts;
            timestamp().revert(time, dts);
            boost::uint64_t offset = seek(dts, delta, ec);
            if (ec) {
                return ec;
            }
            if (buf_.pubseekpos(offset) != std::streampos(offset)) {
                ec = error::file_stream_error;
                return ec;
            }
            time = (boost::uint64_t)-1;
            for (size_t i = 0; i < dts.size(); ++i) {
                boost::uint64_t time2 = timestamp().adjust(i, dts[i]);
                if (time2 < time) {
                    time = time2;
                }
            }
            return ec;
        }

        bool BasicDemuxer::free_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            ec.clear();
            return true;
        }

        boost::uint32_t BasicDemuxer::probe(
            boost::uint8_t const * header, 
            size_t hsize)
        {
            return 0;
        }

        BasicDemuxer * BasicDemuxer::probe(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & content)
        {
            creator_map_type & map(creator_map());
            creator_map_type::const_iterator iter = map.begin();
            creator_map_type::const_iterator max_iter = map.end();
            boost::uint32_t max_scope = 0;
            boost::uint8_t hbytes[32];
            size_t hsize = content.sgetn(hbytes, sizeof(hbytes));
            for (; iter != map.end(); ++iter) {
                BasicDemuxer * demuxer = iter->second(io_svc, content);
                boost::uint32_t scope = demuxer->probe(hbytes, hsize);
                if (scope > max_scope) {
                    max_iter = iter;
                    max_scope = scope;
                }
                delete demuxer;
            }
            content.pubseekpos(0, std::ios::in);
            return max_iter == map.end() ? NULL : max_iter->second(io_svc, content);
        }

        boost::uint64_t BasicDemuxer::get_cur_time(
            boost::system::error_code & ec) const
        {
            if (is_open_) {
                ec.clear();
                return timestamp().time();
            } else {
                ec = error::not_open;
                return 0;
            }
        }

        JointShareInfo * BasicDemuxer::joint_share()
        {
            return NULL;
        }

        void BasicDemuxer::joint_share(
            JointShareInfo * info)
        {
        }

        void BasicDemuxer::on_open()
        {
            assert(!is_open_);
            Demuxer::on_open();
            if (joint_) {
                if (timestamp_) {
                    joint_->read_ctx().begin(*this);
                }
                if (timestamp2_) {
                    joint_->write_ctx().begin(*this);
                }
                if (!joint_->share_info()) {
                    joint_->share_info(joint_share());
                }
            }
            is_open_ = true;
        }

        void BasicDemuxer::on_close()
        {
            assert(is_open_);
            is_open_ = false;
            if (joint_) {
                if (timestamp_) {
                    joint_->read_ctx().end(*this);
                }
                if (timestamp2_) {
                    joint_->write_ctx().end(*this);
                }
            }
            Demuxer::on_close();
        }

        void BasicDemuxer::joint_begin(
            JointContext & context)
        {
            assert(joint_ == NULL || joint_ == &context);
            assert(timestamp_ == NULL);
            joint_ = &context;
            timestamp_ = &joint_->read_ctx().timestamp();
            Demuxer::timestamp(*timestamp_);
            if (joint_->share_info()) {
                joint_share(joint_->share_info());
            }
            if (is_open_) {
                joint_->read_ctx().begin(*this);
                boost::system::error_code ec;
                reset(ec);
                assert(!ec);
            }
        }

        void BasicDemuxer::joint_end()
        {
            joint_->read_ctx().end(*this);
            Demuxer::timestamp(timestamp2_ ? *timestamp2_ : *(TimestampHelper *)NULL);
            timestamp_ = NULL;
            if (timestamp2_ == NULL) {
                joint_ = NULL;
            }
        }

        void BasicDemuxer::joint_begin2(
            JointContext & context)
        {
            assert(joint_ == NULL || joint_ == &context);
            assert(timestamp2_ == NULL);
            joint_ = &context;
            timestamp2_ = &joint_->write_ctx().timestamp();
            Demuxer::timestamp(timestamp_ ? *timestamp_ : *timestamp2_);
            if (joint_->share_info()) {
                joint_share(joint_->share_info());
            }
            if (is_open_) {
                joint_->write_ctx().begin(*this);
            }
        }

        void BasicDemuxer::joint_end2()
        {
            if (!is_open_) { // 必须尝试打开，否则时间戳可能无法连续
                boost::system::error_code ec;
                is_open(ec);
            }
            joint_->write_ctx().end(*this);
            Demuxer::timestamp(timestamp_ ? *timestamp_ : *(TimestampHelper *)NULL);
            timestamp2_ = NULL;
            if (timestamp_ == NULL) {
                joint_ = NULL;
            }
        }

        boost::uint64_t BasicDemuxer::get_joint_cur_time(
            boost::system::error_code & ec) const
        {
            boost::uint64_t t = get_cur_time(ec);
            if (!ec) {
                joint_->read_ctx().last_time(t);
            } else if (ec == error::file_stream_error 
                || ec == error::not_open) {
                    t = joint_->write_ctx().last_time();
            }
            return t;
        }

        boost::uint64_t BasicDemuxer::get_joint_end_time(
            boost::system::error_code & ec)
        {
            boost::uint64_t t = get_end_time(ec);
            if (!ec) {
                joint_->write_ctx().last_time(t);
            } else if (ec == error::file_stream_error) {
                t = joint_->write_ctx().last_time();
            }
            return t;
        }

    } // namespace demux
} // namespace ppbox

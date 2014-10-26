// AviDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/avi/AviDemuxer.h"
#include "ppbox/demux/basic/avi/AviStream.h"

#include <ppbox/avformat/avi/box/AviBox.hpp>
using namespace ppbox::avformat;
using namespace ppbox::avformat::error;

#include <framework/system/BytesOrder.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <stdio.h>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.AviDemuxer", framework::logger::Warn)

namespace ppbox
{
    namespace demux
    {

        AviDemuxer::AviDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_((boost::uint64_t)-1)
            , parse_offset_(0)
            , header_offset_(0)
            , stream_list_(new StreamList)
        {
        }

        AviDemuxer::~AviDemuxer()
        {
            if (stream_list_) {
                delete stream_list_;
                stream_list_ = NULL;
            }
        }

        boost::system::error_code AviDemuxer::open(
            boost::system::error_code & ec)
        {
            open_step_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        boost::system::error_code AviDemuxer::close(
            boost::system::error_code & ec)
        {
            if (open_step_ == 2) {
                on_close();
            }
            open_step_ = boost::uint64_t(-1);
            parse_offset_ = header_offset_ = 0;
            stream_list_->clear();
            for (size_t i = 0; i < streams_.size(); ++i) {
                delete streams_[i];
                streams_.clear();
            }
            file_.close();
            return ec;
        }

        bool AviDemuxer::is_open(
            boost::system::error_code & ec)
        {
            ec.clear();

            if (open_step_ == 2) {
                return true;
            }

            if (open_step_ == (boost::uint64_t)-1) {
                ec = error::not_open;
                return false;
            }

            AviBoxContext ctx;
            archive_.context(&ctx);
            archive_.seekg(parse_offset_, std::ios::beg);
            assert(archive_);

            if (parse_offset_ == 0) {
                AviBoxHeader h;
                if (archive_ >> h)
                    parse_offset_ = archive_.tellg();
            }


            if (box_.get() == NULL)
                box_.reset(new AviBox);
            while (archive_ >> *box_) {
                AviBox * box = box_.release();
                parse_offset_ = archive_.tellg();
                if (file_.open(box, ec)) {
                    open_step_ = 1;
                    break;
                }
                if (ec) {
                    break;
                }
                if (box->id() == AviBoxType::movi) {
                    std::streamoff end = archive_.tellg() + (std::streamoff)box->data_size();
                    header_offset_ = archive_.tellg();
                    archive_.rdbuf()->pubseekoff(end, std::ios::beg, std::ios::in | std::ios::out);
                    if (!archive_)
                        break;
                }
                box_.reset(new AviBox);
            }

            if (open_step_ == 1) {
                std::vector<ppbox::avformat::AviStream *> & streams(file_.header_list()->streams());
                for (std::vector<ppbox::avformat::AviStream *>::iterator iter = streams.begin(); iter != streams.end(); ++iter) {
                    ppbox::avformat::AviStream & stream(**iter);
                    if (stream.type() != AviStreamType::auds
                        && stream.type() != AviStreamType::vids) {
                            continue;
                    }
                    ppbox::demux::AviStream * stream2 = new ppbox::demux::AviStream(streams_.size(), stream, timestamp());
                    if (stream2->parse(ec)) {
                        streams_.push_back(stream2);
                        stream_list_->push(stream2);
                    } else {
                        delete stream2;
                        ec .clear();
                    }
                }
                if (streams_.empty()) {
                    ec = bad_media_format;
                } else {
                    open_step_ = 2;
                    on_open();
                }
            } else {
                if (!archive_) {
                    if (archive_.failed()) {
                        box_.reset();
                        ec = bad_media_format;
                    } else {
                        ec = file_stream_error;
                    }
                    archive_.clear();
                }
            }

            return !ec;
        }

        bool AviDemuxer::is_open(
            boost::system::error_code & ec) const
        {
            if (open_step_ == 2) {
                ec.clear();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        size_t AviDemuxer::get_stream_count(
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                return streams_.size();
            }
        }

        boost::system::error_code AviDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
            } else if (index >= streams_.size()) {
                ec = framework::system::logic_error::out_of_range;
            } else {
                info = *streams_[index];
                info.index = index;
            }
            return ec;
        }

        boost::uint64_t AviDemuxer::get_duration(
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                ec .clear();
                return streams_[0]->duration * 1000 / streams_[0]->time_scale;
            }
        }

        boost::system::error_code AviDemuxer::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }

            framework::timer::TimeCounter tc;

            if (stream_list_->empty()) {
                ec = end_of_stream;
                return ec;
            }

            AviStream & stream = *stream_list_->first();
            stream.get_sample(sample);
            archive_.seekg(sample.time + sample.size, std::ios_base::beg);
            if (!archive_) {
                archive_.clear();
                assert(archive_);
                return ec = file_stream_error;
            }
            stream_list_->pop();

            BasicDemuxer::begin_sample(sample);
            sample.itrack = stream.index;
            sample.stream_info = streams_[sample.itrack];
            BasicDemuxer::push_data(sample.time, sample.size);
            BasicDemuxer::end_sample(sample);

            if (stream.next_sample(ec)) {
                stream_list_->push(&stream);
            }

            ec.clear();

            if (tc.elapse() >= 20) {
                LOG_DEBUG("[get_sample] elapse: " << tc.elapse());
            }

            return ec;
        }

        boost::uint64_t AviDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return boost::uint64_t(-1);
            }

            for (size_t i = 0; i < streams_.size(); ++i) {
                if ((boost::uint64_t)dts[i] > streams_[i]->duration) {
                    ec = framework::system::logic_error::out_of_range;
                    return 0;
                }
            }

            stream_list_->clear();

            AviStream::StreamTimeList stream_time_list;
            AviStream::StreamOffsetList stream_offset_list;

            for (size_t i = 0; i < streams_.size(); ++i) {
                boost::uint64_t time = (boost::uint64_t)dts[i];
                if (streams_[i]->seek(time, ec)) {
                    dts[i] = (boost::uint64_t)time;
                    stream_time_list.push(streams_[i]);
                }
            }
            if (stream_time_list.empty()) {
                return 0;
            }

            AviStream * stream = stream_time_list.first();
            stream_time_list.pop();
            stream_offset_list.push(stream);

            boost::uint64_t min_time = stream->time();
            while ((stream = stream_time_list.first())) {
                stream_time_list.pop();
                boost::uint64_t time = timestamp().revert(stream->index, min_time);
                if ((boost::int64_t)time < 0) {
                    time = 0;
                }
                if (stream->seek(time, ec)) {
                    dts[stream->index] = time;
                    stream_offset_list.push(stream);
                }
            }

            boost::uint64_t seek_offset = stream_offset_list.first()->offset();

            while ((stream = stream_offset_list.first())) {
                stream_offset_list.pop();
                stream_list_->push(stream);
            }

            return seek_offset;
        }

        boost::uint32_t AviDemuxer::probe(
            boost::uint8_t const * header, 
            size_t hsize)
        {
            if (hsize < 12)
                return 0;
            if (memcmp(header, "RIFF", 4) == 0
                && memcmp(header + 8, "AVI ", 4)) {
                    return SCOPE_MAX;
            }
            return 0;
        }

        boost::uint64_t AviDemuxer::get_cur_time(
            boost::system::error_code & ec) const
        {
            boost::uint64_t time = 0;
            if (is_open(ec)) {
                if (stream_list_->empty()) {
                    time = get_duration(ec);
                } else {
                    time = stream_list_->first()->time();
                }
            }
            return time;
        }

        boost::uint64_t AviDemuxer::get_end_time(
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            assert(archive_);
            boost::uint64_t position = archive_.tellg();
            archive_.seekg(0, std::ios_base::end);
            assert(archive_);
            boost::uint64_t offset = archive_.tellg();
            archive_.seekg(position, std::ios_base::beg);
            assert(archive_);

            boost::uint64_t min_time = (boost::uint64_t)-1;
            for (size_t i = 0; i < streams_.size(); ++i) {
                boost::uint64_t time = 0;
                streams_[i]->limit(offset, time, ec);
                time = timestamp().const_adjust(i, time);
                if (time < min_time) {
                    min_time = time;
                }
            }

            ec.clear();
            return min_time;
        }

    } // namespace demux
} // namespace ppbox

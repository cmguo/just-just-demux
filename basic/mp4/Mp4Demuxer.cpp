// Mp4Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/mp4/Mp4Demuxer.h"
#include "ppbox/demux/basic/mp4/Mp4Stream.h"

#include <ppbox/avformat/mp4/box/Mp4Box.hpp>
using namespace ppbox::avformat;
using namespace ppbox::avformat::error;

#include <framework/system/BytesOrder.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.Mp4Demuxer", framework::logger::Warn)

namespace ppbox
{
    namespace demux
    {

        Mp4Demuxer::Mp4Demuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_((boost::uint64_t)-1)
            , parse_offset_(0)
            , header_offset_(0)
            , bitrate_(0)
            , stream_list_(new StreamList)
        {
        }

        Mp4Demuxer::~Mp4Demuxer()
        {
            if (stream_list_) {
                delete stream_list_;
                stream_list_ = NULL;
            }
        }

        boost::system::error_code Mp4Demuxer::open(
            boost::system::error_code & ec)
        {
            open_step_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        boost::system::error_code Mp4Demuxer::close(
            boost::system::error_code & ec)
        {
            if (open_step_ == 1) {
                on_close();
            }
            open_step_ = boost::uint64_t(-1);
            if (stream_list_) {
                delete stream_list_;
                stream_list_ = NULL;
            }
            for (size_t i = 0; i < streams_.size(); ++i) {
                delete streams_[i];
                streams_.clear();
            }
            //if (file_) {
            //    delete file_;
            //    file_ = NULL;
            //}
            return ec;
        }

        bool Mp4Demuxer::is_open(
            boost::system::error_code & ec)
        {
            ec.clear();

            if (open_step_ == 4) {
                return true;
            }

            if (open_step_ == (boost::uint64_t)-1) {
                ec = error::not_open;
                return false;
            }

            Mp4BoxContext ctx;
            archive_.context(&ctx);
            archive_.seekg(parse_offset_, std::ios::beg);
            assert(archive_);

            if (open_step_ == 0) {
                Mp4Box & box(file_.ftyp);
                while (archive_ >> box) {
                    if (box.type != Mp4BoxType::ftyp) {
                        continue;
                    }
                    parse_offset_ = archive_.tellg();
                    open_step_ = 1;
                    break;
                }
            }

            if (open_step_ == 1) {
                Mp4Box & box(file_.moov);
                while (archive_ >> box) {
                    if (box.type == Mp4BoxType::mdat) {
                        file_.mdat = box;
                        parse_offset_ = archive_.tellg() + (std::streamoff)box.data_size();
                        header_offset_ = archive_.tellg() - (std::streamoff)box.head_size();
                        archive_.seekg(parse_offset_, std::ios::beg);
                        if (!archive_)
                            break;
                    }
                    if (box.type != Mp4BoxType::moov) {
                        continue;
                    }
                    parse_offset_ = archive_.tellg();
                    if (header_offset_)
                        open_step_ = 3;
                    else
                        open_step_ = 2;
                    break;
                }
            }

            if (open_step_ == 2) {
                Mp4Box & box(file_.mdat);
                while (archive_ >> box) {
                    if (box.type != Mp4BoxType::mdat) {
                        continue;
                    }
                    parse_offset_ = archive_.tellg();
                    header_offset_ = archive_.tellg() - (std::streamoff)box.head_size();
                    //bitrate_ = (boost::uint64_t)(box.byte_size() * 8 / file->GetMovie()->GetDurationMs());
                    open_step_ = 3;
                    break;
                }
            }

            if (open_step_ == 3) {
                if (file_.parse(ec)) {
                    std::vector<Mp4Track *> & tracks(file_.movie().tracks());
                    for (std::vector<Mp4Track *>::iterator iter = tracks.begin(); iter != tracks.end(); ++iter) {
                        Mp4Track & track(**iter);
                        if (track.type() != Mp4HandlerType::soun
                            && track.type() != Mp4HandlerType::vide) {
                                continue;
                        }
                        Mp4Stream * stream = new Mp4Stream(streams_.size(), track, timestamp());
                        if (stream->parse(timestamp(), ec)) {
                            streams_.push_back(stream);
                        } else {
                            delete stream;
                            ec .clear();
                        }
                    }
                    if (streams_.empty()) {
                        ec = bad_media_format;
                    } else {
                        open_step_ = 4;
                        on_open();
                    }
                }
            } else {
                if (!archive_) {
                    if (archive_.failed()) {
                        ec = bad_media_format;
                    } else {
                        ec = file_stream_error;
                    }
                    archive_.clear();
                }
            }

            return !ec;
        }

        bool Mp4Demuxer::is_open(
            boost::system::error_code & ec) const
        {
            if (open_step_ == 4) {
                ec.clear();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        size_t Mp4Demuxer::get_stream_count(
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                return streams_.size();
            }
        }

        boost::system::error_code Mp4Demuxer::get_stream_info(
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

        boost::uint64_t Mp4Demuxer::get_duration(
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                ec .clear();
                return file_.movie().duration() * 1000 / file_.movie().time_scale();
            }
        }

        boost::system::error_code Mp4Demuxer::get_sample(
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

            Mp4Stream & stream = *stream_list_->first();
            stream.get_sample(sample);
            archive_.seekg(sample.time + sample.size, std::ios_base::beg);
            if (!archive_) {
                archive_.clear();
                assert(archive_);
                return ec = file_stream_error;
            }
            stream_list_->pop();

            BasicDemuxer::begin_sample(sample);
            sample.itrack = stream.itrack();
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

        boost::uint64_t Mp4Demuxer::seek(
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

            Mp4Stream::StreamTimeList stream_time_list;
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

            Mp4Stream::StreamOffsetList stream_offset_list;
            Mp4Stream * stream = stream_time_list.first();
            stream_time_list.pop();
            stream_offset_list.push(stream);

            boost::uint64_t min_time = stream->time();
            while ((stream = stream_time_list.first())) {
                stream_time_list.pop();
                boost::uint64_t time = timestamp().revert(stream->itrack(), min_time);
                if (stream->seek(time, ec)) {
                    dts[stream->itrack()] = time;
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

        boost::system::error_code Mp4Demuxer::reset2(
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                stream_list_->clear();
                //boost::uint64_t min_offset = head_offset_;
                //for (size_t i = 0; i < streams_.size(); ++i) {
                //    streams_[i]->Rewind();
                //    if (AP4_SUCCEEDED(streams_[i]->GetNextSample())) {
                //        streams_[i]->sample_.time = timestamp().const_adjust(i, streams_[i]->sample_.GetDts());
                //        stream_list_->push(&streams_[i]->sample_);
                //        if (streams_[i]->sample_.GetOffset() < min_offset) {
                //            min_offset = streams_[i]->sample_.GetOffset();
                //        }
                //    }
                //}
            }
            return ec;
        }

        boost::uint32_t Mp4Demuxer::probe(
            boost::uint8_t const * header, 
            size_t hsize) const
        {
            if (hsize < 12)
                return 0;
            if (memcmp(header + 4, "ftypisom", 8) == 0) {
                return SCOPE_MAX;
            }
            return 0;
        }

        boost::uint64_t Mp4Demuxer::get_cur_time(
            boost::system::error_code & ec) const
        {
            boost::uint64_t dts = 0;
            if (is_open(ec)) {
                //if (stream_list_->empty()) {
                //    dts = (*streams_[0])->GetMediaDuration();
                //    dts = timestamp().const_adjust(0, dts);
                //} else {
                //    dts = stream_list_->first()->GetDts();
                //    dts = timestamp().const_adjust(stream_list_->first()->itrack, dts);
                //}
            }
            return dts;
        }

        boost::uint64_t Mp4Demuxer::get_end_time(
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


            boost::uint64_t time_hint = 0;
            if (bitrate_ != 0 && offset > header_offset_) {
                time_hint = (boost::uint64_t)((offset - header_offset_) * 8 * 1000 / bitrate_);
            }

            boost::uint64_t min_time = (boost::uint64_t)-1;
            //for (size_t i = 0; i < streams_.size(); ++i) {
            //    boost::uint64_t time = AP4_ConvertTime(time_hint, 1000, (*streams_[i])->GetMediaTimeScale());
            //    if (AP4_SUCCEEDED(streams_[i]->GetBufferTime(offset, time))) {
            //        time = timestamp().const_adjust(i, time);
            //        if (time < min_time) {
            //            min_time = time;
            //        }
            //    }
            //}

            ec.clear();
            return min_time;
        }

    } // namespace demux
} // namespace ppbox

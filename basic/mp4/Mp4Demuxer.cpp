// Mp4Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/mp4/Mp4Demuxer.h"
#include "ppbox/demux/basic/mp4/Mp4Track.h"
#include "ppbox/demux/basic/mp4/Mp4StdByteStream.h"

#include <ppbox/avformat/mp4/Mp4Algorithm.h>
using namespace ppbox::avformat;

#include <util/buffers/BufferCopy.h>
#include <util/buffers/BufferSize.h>
using namespace util::buffers;

#include <framework/system/BytesOrder.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

using namespace boost::asio;

#include <fstream>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.Mp4Demuxer", framework::logger::Warn)

namespace ppbox
{
    namespace demux
    {

        Mp4Demuxer::Mp4Demuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , is_(& buf)
            , head_size_(24)
            , open_step_((boost::uint64_t)-1)
            , file_(NULL)
            , bitrate_(0)
            , sample_list_(NULL)
        {
        }

        Mp4Demuxer::~Mp4Demuxer()
        {
            if (sample_list_) {
                delete sample_list_;
                sample_list_ = NULL;
            }
            for (size_t i = 0; i < tracks_.size(); ++i) {
                delete tracks_[i];
            }
            if (file_) {
                delete file_;
                file_ = NULL;
            }
        }

        error_code Mp4Demuxer::open(
            error_code & ec)
        {
            open_step_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        error_code Mp4Demuxer::close(
            error_code & ec)
        {
            if (open_step_ == 1) {
                on_close();
            }
            open_step_ = boost::uint64_t(-1);
            if (sample_list_) {
                delete sample_list_;
                sample_list_ = NULL;
            }
            for (size_t i = 0; i < tracks_.size(); ++i) {
                delete tracks_[i];
                tracks_.clear();
            }
            if (file_) {
                delete file_;
                file_ = NULL;
            }
            return ec;
        }

        bool Mp4Demuxer::is_open(
            boost::system::error_code & ec)
        {
            if (open_step_ == 1) {
                ec= error_code();
                return true;
            }

            if (open_step_ == (boost::uint64_t)-1) {
                ec = error::not_open;
                return false;
            }

            if (open_step_ == 0) {
                head_size_ = mp4_head_size(is_, ec);
                if (!ec) {
                    parse_head(ec);
                    if (!ec) {
                        open_step_ = 1;
                        on_open();
                        reset2(ec);
                        assert(!ec);
                    }
                }
            }
            return !ec;
        }

        bool Mp4Demuxer::is_open(
            boost::system::error_code & ec) const
        {
            if (open_step_ == 1) {
                ec= error_code();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        error_code Mp4Demuxer::parse_head(
            error_code & ec)
        {
            LOG_DEBUG("begin parse head,size: " << head_size_);

           /* if (min_head_size(buf) > util::buffers::buffer_size(buf)) {
                return ec = bad_offset_size;
            }*/

            /*{
                std::vector<char> vec(buf.in_avail(), 0);
                util::buffers::buffer_copy(boost::asio::buffer(vec), buf.data());
                std::ofstream ofs("head.mp4", std::ios::binary);
                ofs.write(&vec.at(0), vec.size());
            }*/

            AP4_ByteStream * memStream = new Mp4StdByteStream(&is_);
            //AP4_ByteStream * memStream = new_buffer_byte_stream(buf);
            AP4_File * file = new AP4_File(*memStream);
            memStream->Release();
            if (!file) {
                ec = no_momery;
                return ec;
            }

            if (file->GetMovie() == NULL) {
                delete file;
                ec = bad_file_format;
                return ec;
            }

            AP4_Atom * atom_mdat = file->FindChild("mdat");
            if (atom_mdat) {
                bitrate_ = (boost::uint64_t)(atom_mdat->GetSize() * 8 / file->GetMovie()->GetDurationMs());
            } else {
                AP4_UI32 size_mdat = 0;
                AP4_Atom::Type type_mdat = 0;
                if (AP4_SUCCEEDED(memStream->ReadUI32(size_mdat)) && 
                    AP4_SUCCEEDED(memStream->ReadUI32(type_mdat)) ) {
                        atom_mdat = new AP4_UnknownAtom(type_mdat, size_mdat, *memStream);
                        atom_mdat->SetSize32(size_mdat);
                        file->AddChild(atom_mdat);
                        bitrate_ = size_mdat * 8 / file->GetMovie()->GetDurationMs();
                } else {
                    delete file;
                    ec = bad_file_format;
                    return ec;
                }
            }

            AP4_Movie* movie = file->GetMovie();
            if (!movie || movie->GetTracks().ItemCount() == 0) {
                delete file;
                ec = bad_file_format;
                return ec;
            }

            ec .clear();
            AP4_List<AP4_Track> & tracks = file->GetMovie()->GetTracks();
            for (AP4_List<AP4_Track>::Item * item = tracks.FirstItem(); item; item = item->GetNext()) {
                Track * track = new Track(tracks_.size(), item->GetData(), head_size_, ec);
                if (ec) {
                    delete track;
                    ec .clear();
                } else {
                    tracks_.push_back(track);
                }
            }

            if (tracks_.empty()) {
                ec = bad_file_format;
            }

            if (ec) {
                delete file;
                return ec;
            }
/*
            SampleOffsetList sample_offset_list;
            boost::uint64_t last_offset = head_size_;
            for (size_t i = 0; i < tracks_.size(); ++i) {
                // 第一个Sample已经在Mp4Track构造时获取了
                sample_offset_list.push(&tracks_[i]->sample_);
            }
            while (!sample_offset_list.empty()) {
                SampleListItem & ap4_sample = *sample_offset_list.first();
                sample_offset_list.pop();
                if (ap4_sample.GetOffset() < last_offset) {
                    ec = bad_smaple_order;
                    LOG_DEBUG("bad smaple order: " << ap4_sample.GetOffset() << "<" << last_offset);
                    break;
                } else if (ap4_sample.GetOffset() > last_offset) {
                    LOG_DEBUG("skip data: " << last_offset << "(" << ap4_sample.GetOffset() - last_offset << ")");
                }
                last_offset = ap4_sample.GetOffset() + ap4_sample.GetSize();
                Track * track = tracks_[ap4_sample.itrack];
                if (AP4_SUCCEEDED(track->GetNextSample())) {
                    sample_offset_list.push(&track->sample_);
                }
            }
            if (last_offset < total_size) {
                LOG_DEBUG("skip to end: " << last_offset << "(" << total_size - last_offset << ")");
            }

            if (ec) {
                delete file;
                return ec;
            }
*/
            file_ = file;

            sample_list_ = new SampleList;

            return ec;
        }

        size_t Mp4Demuxer::get_stream_count(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                ec = error_code();
                return tracks_.size();
            }
        }

        boost::system::error_code Mp4Demuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            boost::system::error_code & ec) const
        {
            if (!is_open(ec)) {
            } else if (index >= tracks_.size()) {
                ec = out_of_range;
            } else {
                info = *tracks_[index];
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
                ec = error_code();
                return file_->GetMovie()->GetDurationMs();
            }
        }

        error_code Mp4Demuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }

            framework::timer::TimeCounter tc;

            if (sample_list_->empty()) {
                ec = no_more_sample;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    assert(tracks_[i]->next_index_ == tracks_[i]->total_index_);
                }
                return ec;
            }

            SampleListItem & ap4_sample = *sample_list_->first();

            is_.seekg(ap4_sample.GetOffset() + ap4_sample->GetSize(), std::ios_base::beg);
            if (!is_) {
                is_.clear();
                assert(is_);
                return ec = error::file_stream_error;
            }

            sample_list_->pop();
            BasicDemuxer::begin_sample(sample);
            sample.itrack = ap4_sample.itrack;
            sample.flags = 0;
            if (ap4_sample.IsSync())
                sample.flags |= Sample::sync;
            //sample.time = ap4_sample.time;
            //sample.ustime = ap4_sample.ustime;
            sample.dts = ap4_sample.GetDts();
            sample.cts_delta = ap4_sample.GetCtsDelta();
            sample.duration = ap4_sample.GetDuration();
            sample.size = ap4_sample.GetSize();
            BasicDemuxer::push_data(ap4_sample.GetOffset(), ap4_sample.GetSize());
            BasicDemuxer::end_sample(sample);

            Track * track = tracks_[ap4_sample.itrack];
            if (AP4_SUCCEEDED(track->GetNextSample())) {
                track->sample_.time = timestamp().const_adjust(ap4_sample.itrack, track->sample_.GetDts());
                sample_list_->push(&track->sample_);
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

            for (size_t i = 0; i < tracks_.size(); ++i) {
                if ((AP4_UI64)dts[i] > (*tracks_[i])->GetMediaDuration()) {
                    ec = framework::system::logic_error::out_of_range;
                    return 0;
                }
            }

            std::vector<boost::uint64_t> offset;
            sample_list_->clear();
            for (size_t i = 0; i < tracks_.size(); ++i) {
                AP4_UI64 time = (AP4_UI64)dts[i];
                AP4_Position off = 0;
                if (AP4_SUCCEEDED(tracks_[i]->Seek(time, off))) {
                    dts[i] = (boost::uint64_t)time;
                    offset.push_back(off);
                }
            }

            AP4_UI64 min_time = (AP4_UI64)-1;
            AP4_Position seek_offset = (AP4_Position)-1;
            size_t min_time_index  = 0;

            for (size_t i = 0; i < tracks_.size(); ++i) {
                AP4_UI64 time = AP4_ConvertTime(dts[i], (*tracks_[i])->GetMediaTimeScale(), 1000);
                if (time < min_time) {
                    min_time = time;
                    seek_offset = offset[i];
                    min_time_index = i;
                }
            }

            for (size_t i = 0; i < tracks_.size(); ++i) {
                AP4_Position seek_offset1 = seek_offset;
                if (i == min_time_index) {
                    tracks_[i]->sample_.time = timestamp().const_adjust(i, tracks_[i]->sample_.GetDts());
                    sample_list_->push(&tracks_[i]->sample_);
                    continue;
                }
                AP4_UI64 time = dts[i] = AP4_ConvertTime(min_time, 1000, (*tracks_[i])->GetMediaTimeScale());
                AP4_Position off = 0;
                if (AP4_SUCCEEDED(tracks_[i]->Seek(time, off))) {
                    tracks_[i]->sample_.time = timestamp().const_adjust(i, tracks_[i]->sample_.GetDts());
                    sample_list_->push(&tracks_[i]->sample_);
                    if (off < seek_offset) {
                        seek_offset = off;
                    }
                }
            }

            if (seek_offset == 0) {
                ec = framework::system::logic_error::out_of_range;
            } else {
                ec = error_code();
            }
            return seek_offset;
        }

        error_code Mp4Demuxer::reset2(
            error_code & ec)
        {
            if (!file_) {
                ec = not_open;
            } else {
                sample_list_->clear();
                boost::uint64_t min_offset = head_size_;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    tracks_[i]->Rewind();
                    if (AP4_SUCCEEDED(tracks_[i]->GetNextSample())) {
                        tracks_[i]->sample_.time = timestamp().const_adjust(i, tracks_[i]->sample_.GetDts());
                        sample_list_->push(&tracks_[i]->sample_);
                        if (tracks_[i]->sample_.GetOffset() < min_offset) {
                            min_offset = tracks_[i]->sample_.GetOffset();
                        }
                    }
                }
                ec = error_code();
            }
            return ec;
        }

        boost::uint64_t Mp4Demuxer::get_end_time(
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            assert(is_);
            boost::uint64_t position = is_.tellg();
            is_.seekg(0, std::ios_base::end);
            assert(is_);
            boost::uint64_t offset = is_.tellg();
            is_.seekg(position, std::ios_base::beg);
            assert(is_);


            AP4_UI64 time_hint = 0;
            if (bitrate_ != 0 && offset > head_size_) {
                time_hint = (AP4_UI64)((offset - head_size_) * 8 * 1000 / bitrate_);
            }

            AP4_UI64 min_time = (AP4_UI64)-1;
            for (size_t i = 0; i < tracks_.size(); ++i) {
                AP4_UI64 time = AP4_ConvertTime(time_hint, 1000, (*tracks_[i])->GetMediaTimeScale());
                if (AP4_SUCCEEDED(tracks_[i]->GetBufferTime(offset, time))) {
                    time = timestamp().const_adjust(i, time);
                    if (time < min_time) {
                        min_time = time;
                    }
                }
            }

            ec = error_code();
            return min_time;
        }

        boost::uint64_t Mp4Demuxer::get_cur_time(
            error_code & ec) const
        {
            boost::uint64_t dts = 0;
            if (is_open(ec)) {
                if (sample_list_->empty()) {
                    dts = (*tracks_[0])->GetMediaDuration();
                    dts = timestamp().const_adjust(0, dts);
                } else {
                    dts = sample_list_->first()->GetDts();
                    dts = timestamp().const_adjust(sample_list_->first()->itrack, dts);
                }
            }
            return dts;
        }

    } // namespace demux
} // namespace ppbox

// Mp4DemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/mp4/Mp4Track.h"
#include "ppbox/demux/mp4/Mp4BytesStream.h"
#include "ppbox/demux/mp4/Mp4StdByteStream.h"

#include <util/buffers/BufferCopy.h>
#include <util/buffers/BufferSize.h>
using namespace util::buffers;

#include <framework/system/BytesOrder.h>
#include <framework/timer/TimeCounter.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;

using namespace boost::asio;

#include <fstream>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("Mp4DemuxerBase", 0)

namespace ppbox
{
    namespace demux
    {

        Mp4DemuxerBase::Mp4DemuxerBase(
            std::basic_streambuf<boost::uint8_t> & buf)
            : DemuxerBase(buf)
            , is_(& buf)
            , head_size_(24)
            , open_step_((boost::uint32_t)-1)
            , file_(NULL)
            , bitrate_(0)
            , sample_list_(NULL)
            , sample_put_back_(false)
            , min_offset_(0)
        {
        }

        Mp4DemuxerBase::Mp4DemuxerBase(
            Mp4DemuxerBase * from, 
            std::basic_streambuf<boost::uint8_t> & buf, 
            boost::uint32_t head_size_, 
            boost::uint32_t open_step_, 
            AP4_File * file_, 
            boost::uint32_t bitrate_, 
            boost::uint64_t min_offset_)
            : DemuxerBase(buf)
            , is_(& buf)
            , head_size_(from->head_size_)
            , open_step_(from->open_step_)
            , file_(from->file_)
            , bitrate_(from->bitrate_)
            , sample_list_(new SampleList)
            , sample_put_back_(false)
            , min_offset_(from->min_offset_)
        {
            for (size_t i = 0; i < from->tracks_.size(); ++i) {
                tracks_.push_back(from->tracks_[i]);
            }
        }

        Mp4DemuxerBase::~Mp4DemuxerBase()
        {
            if (sample_list_)
                delete sample_list_;
            for (size_t i = 0; i < tracks_.size(); ++i) {
                delete tracks_[i];
            }
            if (file_)
                delete file_;
        }

        Mp4DemuxerBase * Mp4DemuxerBase::clone(
            std::basic_streambuf<boost::uint8_t> & buf)
        {
            Mp4DemuxerBase * demuxer = new Mp4DemuxerBase(this, buf, head_size_, open_step_, file_, 
                bitrate_, min_offset_);
            return demuxer;
        }

        error_code Mp4DemuxerBase::open(
            error_code & ec)
        {
            open_step_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        bool Mp4DemuxerBase::is_open(
            boost::system::error_code & ec)
        {
            if (open_step_ == 1) {
                ec= error_code();
                return true;
            }

            if (open_step_ == (boost::uint32_t)-1) {
                ec = error::not_open;
                return false;
            }

            if (open_step_ == 0) {
                size_t length = 0;
                is_.seekg(0, std::ios_base::end);
                length = is_.tellg();
                is_.seekg(0, std::ios_base::beg);
                if (length < 24) {
                    ec = error::file_stream_error;
                    return false;
                }
                boost::uint8_t size1_char;
                is_.read(&size1_char, 4);
                boost::uint32_t size1 = BytesOrder::host_to_net_long(* (boost::uint32_t *)&size1_char);
                is_.seekg(0, std::ios_base::end);
                length = is_.tellg();
                is_.seekg(0, std::ios_base::beg);
                if (length < size1 + 16) {
                    head_size_ = size1 + 16;
                    ec = error::file_stream_error;
                    return false;
                }
                is_.seekg(size1, std::ios_base::beg);
                boost::uint8_t size2_char;
                is_.read(&size2_char, 4);
                boost::uint32_t size2 = BytesOrder::host_to_net_long(* (boost::uint32_t *)&size2_char);
                is_.seekg(0, std::ios_base::end);
                length = is_.tellg();
                is_.seekg(0, std::ios_base::beg);
                head_size_ = size1 + size2 + 8;
                if (length < size1 + size2 + 8) {
                    ec = error::file_stream_error;
                    return false;
                }
                is_.seekg(0, std::ios_base::beg);
                parse_head(ec);
                if (ec) {
                    return false;
                } else {
                    open_step_ = 1;
                    return true;
                }
            }
            return !ec;
        }

        error_code Mp4DemuxerBase::parse_head(
            error_code & ec)
        {
            LOG_S(Logger::kLevelDebug, "begin parse head,size: " << head_size_);

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
                bitrate_ = (boost::uint32_t)(atom_mdat->GetSize() * 8 / file->GetMovie()->GetDurationMs());
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

            ec = error_code();
            AP4_List<AP4_Track> & tracks = file->GetMovie()->GetTracks();
            for (AP4_List<AP4_Track>::Item * item = tracks.FirstItem(); item; item = item->GetNext()) {
                Track * track = new Track(tracks_.size(), item->GetData(), head_size_, ec);
                if (ec)
                    break;
                tracks_.push_back(track);
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
                    LOG_S(Logger::kLevelDebug, "bad smaple order: " << ap4_sample.GetOffset() << "<" << last_offset);
                    break;
                } else if (ap4_sample.GetOffset() > last_offset) {
                    LOG_S(Logger::kLevelDebug, "skip data: " << last_offset << "(" << ap4_sample.GetOffset() - last_offset << ")");
                }
                last_offset = ap4_sample.GetOffset() + ap4_sample.GetSize();
                Track * track = tracks_[ap4_sample.itrack];
                if (AP4_SUCCEEDED(track->GetNextSample())) {
                    sample_offset_list.push(&track->sample_);
                }
            }
            if (last_offset < total_size) {
                LOG_S(Logger::kLevelDebug, 
                    "skip to end: " << last_offset << "(" << total_size - last_offset << ")");
            }

            if (ec) {
                delete file;
                return ec;
            }
*/
            file_ = file;

            sample_list_ = new SampleList;
            rewind(ec);

            return ec;
        }

        size_t Mp4DemuxerBase::get_media_count(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                ec = error_code();
                return file_->GetMovie()->GetTracks().ItemCount();
            }
        }

        boost::system::error_code Mp4DemuxerBase::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
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

        boost::system::error_code Mp4DemuxerBase::get_track_base_info(
            size_t index, 
            MediaInfoBase & info, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                ec = not_open;
            } else if (index >= tracks_.size()) {
                ec = out_of_range;
            } else {
                info = *tracks_[index];
            }
            return ec;
        }

        boost::uint32_t Mp4DemuxerBase::get_duration(
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            } else {
                ec = error_code();
                return file_->GetMovie()->GetDurationMs();
            }
        }

        error_code Mp4DemuxerBase::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }

            framework::timer::TimeCounter tc;

            if (!sample_put_back_) {
                SampleListItem & ap4_sample = *sample_list_->first();
                sample_list_->pop();
                Track * track = tracks_[ap4_sample.itrack];
                if (AP4_SUCCEEDED(track->GetNextSample())) {
                    sample_list_->push(&track->sample_);
                }
            }

            if (sample_list_->empty()) {
                ec = no_more_sample;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    assert(tracks_[i]->next_index_ == tracks_[i]->total_index_);
                }
                sample_put_back_ = true;
                return ec;
            }

            SampleListItem & ap4_sample = *sample_list_->first();
            ec = error_code();
            sample.itrack = ap4_sample.itrack;
            sample.idesc = ap4_sample.GetDescriptionIndex();
            sample.flags = 0;
            if (ap4_sample.IsSync())
                sample.flags |= Sample::sync;
            sample.time = ap4_sample.time;
            sample.ustime = ap4_sample.ustime;
            sample.dts = ap4_sample.GetDts();
            sample.cts_delta = ap4_sample.GetCtsDelta();
            sample.size = ap4_sample.GetSize();
            sample.blocks.clear();
            sample.blocks.push_back(FileBlock(ap4_sample.GetOffset(), ap4_sample.GetSize()));

            sample_put_back_ = false;
            min_offset_ = ap4_sample.GetOffset();
#ifndef PPBOX_DEMUX_MP4_NO_TIME_ORDER
            for (SampleListItem * sample = sample_list_->next(&ap4_sample); sample; sample = sample_list_->next(sample)) {
                if (sample->GetOffset() < min_offset_) {
                    min_offset_ = sample->GetOffset();
                }
            }
#endif
            is_.seekg(ap4_sample.GetOffset() + sample.size, std::ios_base::beg);
            if (!is_) {
                is_.clear();
                ec = error::file_stream_error;
                sample_put_back_ = true;
            }
            is_.seekg(min_offset_, std::ios_base::beg);
            if (tc.elapse() > 10) {
                LOG_S(Logger::kLevelDebug, "[get_sample] elapse: " << tc.elapse());
            }

            return ec;
        }

        error_code Mp4DemuxerBase::put_back_sample(
            Sample const & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                ec = not_open;
            } else if (sample_list_->empty() || sample_list_->first()->itrack != sample.itrack) {
                ec = item_not_exist;
            } else {
                sample_put_back_ = true;
                ec = error_code();
            }
            return ec;
        }

        boost::uint64_t Mp4DemuxerBase::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return boost::uint64_t(-1);
            }
            if (time > get_duration(ec))
            {
                ec = framework::system::logic_error::out_of_range;
                return 0;
            }
            AP4_UI32 seek_time = time + 1;
            AP4_Position seek_offset = (AP4_Position)-1;
            {
                AP4_UI32 seek_time1 = time;
                AP4_Position seek_offset1 = 0;
                size_t min_time_index  = 0;
                sample_list_->clear();
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    if (AP4_SUCCEEDED(tracks_[i]->Seek(seek_time1 = time, seek_offset1))) {
                        if (seek_time1 < seek_time) {
                            seek_time = seek_time1;
                            seek_offset = seek_offset1;
                            min_time_index = i;
                        }
                    }
                }
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    if (i == min_time_index || 
                        AP4_SUCCEEDED(tracks_[i]->Seek(seek_time1 = seek_time, seek_offset1))) {
                            sample_list_->push(&tracks_[i]->sample_);
                            if (seek_offset1 < seek_offset) {
                                seek_offset = seek_offset1;
                            }
                    }
                }
                sample_put_back_ = true;
            }
            if (seek_offset == 0) {
                ec = framework::system::logic_error::out_of_range;
            } else {
                min_offset_ = seek_offset;
                time = seek_time;
                ec = error_code();
            }
            return seek_offset;
        }

        error_code Mp4DemuxerBase::rewind(
            error_code & ec)
        {
            if (!is_open(ec)) {
                ec = not_open;
            } else {
                sample_list_->clear();
                boost::uint64_t min_offset = head_size_;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    tracks_[i]->Rewind();
                    if (AP4_SUCCEEDED(tracks_[i]->GetNextSample())) {
                        sample_list_->push(&tracks_[i]->sample_);
                        if (tracks_[i]->sample_.GetOffset() < min_offset) {
                            min_offset = tracks_[i]->sample_.GetOffset();
                        }
                    }
                }
                min_offset_ = min_offset;
                sample_put_back_ = true;
                ec = error_code();
            }
            return ec;
        }

        boost::uint32_t Mp4DemuxerBase::get_end_time(
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            size_t position = is_.tellg();
            is_.seekg(0, std::ios_base::end);
            size_t offset = is_.tellg();
            is_.seekg(position, std::ios_base::beg);
            AP4_UI32 time = get_duration(ec);
            if (ec) {
                return 0;
            }
            AP4_UI32 time_hint = (AP4_UI32)((offset - head_size_) * 8 / bitrate_);
            {
                AP4_UI32 time1;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    if (AP4_SUCCEEDED(tracks_[i]->GetBufferTime(offset, time_hint, time1)) && time1 < time) {
                        time = time1;
                    }
                }
            }
            ec = error_code();
            return time;
        }

        boost::uint32_t Mp4DemuxerBase::get_cur_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            } else if (sample_list_->empty()) {
                ec = error_code();
                return file_->GetMovie()->GetDurationMs();
            } else {
                ec = error_code();
                return sample_list_->first()->time;
            }
        }

        boost::uint64_t Mp4DemuxerBase::get_offset(
            boost::uint32_t time, 
            boost::uint32_t & delta, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            if (time > get_duration(ec))
            {
                ec = framework::system::logic_error::out_of_range;
                return 0;
            }
            AP4_UI32 seek_time = time + 1;
            AP4_Position seek_offset = (AP4_Position)-1;
            SampleListItem sample;
            AP4_Ordinal next_index = 0;
            {
                AP4_UI32 seek_time1 = time;
                size_t min_time_index  = 0;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    if (AP4_SUCCEEDED(tracks_[i]->Seek(seek_time1 = time, next_index, sample))) {
                        if (seek_time1 < seek_time) {
                            seek_time = seek_time1;
                            seek_offset = sample.GetOffset();
                            min_time_index = i;
                        }
                    }
                }
                delta = seek_offset; // 记录offset最大值
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    if (i == min_time_index || 
                        AP4_SUCCEEDED(tracks_[i]->Seek(seek_time1 = seek_time, next_index, sample))) {
                            if (sample.GetOffset() < seek_offset) {
                                seek_offset = sample.GetOffset();
                            } else if (sample.GetOffset() > delta) {
                                delta = sample.GetOffset();
                            }
                    }
                }
                sample_put_back_ = true;
            }
            if (seek_offset == 0) {
                ec = framework::system::logic_error::out_of_range;
            } else {
                min_offset_ = seek_offset;
                time = seek_time;
                ec = error_code();
                delta -= seek_offset;
                seek_offset += delta;
            }
            return seek_offset;
        }

    } // namespace demux
} // namespace ppbox

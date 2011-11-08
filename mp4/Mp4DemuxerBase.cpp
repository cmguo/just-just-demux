// Mp4DemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/mp4/Mp4DemuxerBase.h"
#include "ppbox/demux/mp4/Mp4Track.h"
#include "ppbox/demux/mp4/Mp4BytesStream.h"

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

        Mp4DemuxerBase::Mp4DemuxerBase()
            : head_size_(24)
            , bitrate_(0)
            , file_(NULL)
            , sample_list_(NULL)
            , sample_put_back_(false)
            , min_offset_(0)
        {
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

        size_t Mp4DemuxerBase::min_head_size(
            buffer_t const & buf)
        {
            size_t size = util::buffers::buffer_size(buf);
            if (size < head_size_)
                return head_size_;
            if (size < 24) {
                return head_size_ = 24;
            }
            boost::uint32_t size1;
            buffer_copy(buffer(&size1, 4), buf, 4);
            size1 = BytesOrder::host_to_net_long(size1);
            if (size < size1 + 16) {
                return head_size_ = size1 + 16;
            }
            boost::uint32_t size2;
            buffer_copy(buffer(&size2, 4), buf, 4, 0, size1);
            size2 = BytesOrder::host_to_net_long(size2);
            return head_size_ = size1 + size2 + 8;
        }

        error_code Mp4DemuxerBase::set_head(
            buffer_t const & buf, 
            boost::uint64_t total_size, 
            error_code & ec)
        {
            LOG_S(Logger::kLevelDebug, "begin parse head,size: " << head_size_);

            if (min_head_size(buf) > util::buffers::buffer_size(buf)) {
                return ec = bad_offset_size;
            }

            /*{
                std::vector<char> vec(buf.in_avail(), 0);
                util::buffers::buffer_copy(boost::asio::buffer(vec), buf.data());
                std::ofstream ofs("head.mp4", std::ios::binary);
                ofs.write(&vec.at(0), vec.size());
            }*/

            AP4_ByteStream * memStream = new_buffer_byte_stream(buf);
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
                Track * track = new Track(tracks_.size(), item->GetData(), head_size_, total_size, ec);
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

        error_code Mp4DemuxerBase::get_head(
            std::vector<boost::uint8_t> & head_buf, 
            error_code & ec)
        {
            if (!file_) {
                ec = not_open;
                return ec;
            }
            head_buf.resize(head_size_);
            AP4_DataBuffer buffer;
            buffer.SetBuffer(&head_buf.front(), head_size_);
            AP4_MemoryByteStream * stream = new AP4_MemoryByteStream(buffer);
            AP4_List<AP4_Atom> & children = file_->GetChildren();
            for (AP4_List<AP4_Atom>::Item * p = children.FirstItem(); p; p = p->GetNext()) {
                p->GetData()->Write(*stream);
            }
            AP4_Position position = 0;
            (void)position;
            assert(AP4_SUCCEEDED(stream->Tell(position)) && position == head_size_);
            stream->Release();
            return ec = error_code();
        }

        size_t Mp4DemuxerBase::get_media_count(
            error_code & ec)
        {
            if (!file_) {
                ec = not_open;
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
            if (!file_) {
                ec = not_open;
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
            if (!file_) {
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
            if (!file_) {
                ec = not_open;
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
            if (!file_) {
                ec = not_open;
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
            sample.blocks.clear();
            sample.data.clear();
            sample.blocks.push_back(FileBlock(ap4_sample.GetOffset(), ap4_sample.GetSize()));
            sample.size = ap4_sample.GetSize();
            sample.idesc = ap4_sample.GetDescriptionIndex();
            sample.dts = ap4_sample.GetDts();
            sample.cts_delta = ap4_sample.GetCtsDelta();
            sample.is_sync = ap4_sample.IsSync();
            sample.ustime = ap4_sample.ustime;
            sample.time = ap4_sample.time;

            sample_put_back_ = false;
            min_offset_ = ap4_sample.GetOffset();
#ifndef PPBOX_DEMUX_MP4_NO_TIME_ORDER
            for (SampleListItem * sample = sample_list_->next(&ap4_sample); sample; sample = sample_list_->next(sample)) {
                if (sample->GetOffset() < min_offset_) {
                    min_offset_ = sample->GetOffset();
                }
            }
#endif
            if (tc.elapse() > 10) {
                LOG_S(Logger::kLevelDebug, "[get_sample] elapse: " << tc.elapse());
            }

            return ec;
        }

        error_code Mp4DemuxerBase::put_back_sample(
            Sample const & sample, 
            error_code & ec)
        {
            if (!file_) {
                ec = not_open;
            } else if (sample_list_->empty() || sample_list_->first()->itrack != sample.itrack) {
                ec = item_not_exist;
            } else {
                sample_put_back_ = true;
                ec = error_code();
            }
            return ec;
        }

        boost::uint64_t Mp4DemuxerBase::seek_to(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            if (!file_) {
                ec = not_open;
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
            if (!file_) {
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
            boost::uint64_t offset, 
            boost::system::error_code & ec)
        {
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
            if (!file_) {
                ec = not_open;
                return 0;
            } else if (sample_list_->empty()) {
                ec = error_code();
                return file_->GetMovie()->GetDurationMs();
            } else {
                ec = error_code();
                return sample_list_->first()->time;
            }
        }

        void Mp4DemuxerBase::release(void)
        {
            if (sample_list_) {
                delete sample_list_;
                sample_list_ = NULL;
            }
            for (size_t i = 0; i < tracks_.size(); ++i) {
                delete tracks_[i];
            }
            tracks_.clear();
            if (file_) {
                delete file_;
                file_ = NULL;
            }
        }

    } // namespace demux
} // namespace ppbox

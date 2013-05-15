// Mp4Track.h

#ifndef _PPBOX_DEMUX_BASIC_MP4_MP4_TRACK_H_
#define _PPBOX_DEMUX_BASIC_MP4_MP4_TRACK_H_

using namespace ppbox::demux::error;

#include <ppbox/avcodec/Format.h>
using namespace ppbox::avcodec;

#include <framework/container/OrderedUnidirList.h>
using namespace framework::container;
using namespace framework::system;
using namespace framework::system::logic_error;

using namespace boost::system;

#include <bento4/Core/Ap4Atom.h>
#include <bento4/Core/Ap4ByteStream.h>
#include <bento4/Core/Ap4File.h>
#include <bento4/Core/Ap4Movie.h>
#include <bento4/Core/Ap4Sample.h>
#include <bento4/Core/Ap4SampleDescription.h>
#include <bento4/Core/Ap4Track.h>
#include <bento4/Core/Ap4SampleTable.h>
#include <bento4/Core/Ap4Protection.h>
#include <bento4/Core/Ap4AvccAtom.h>
#include <bento4/Core/Ap4TrakAtom.h>
#include <bento4/Core/Ap4StsdAtom.h>
#include <bento4/Core/Ap4HdlrAtom.h>
#include <bento4/Core/Ap4DataBuffer.h>
#include <bento4/Core/Ap4StscAtom.h>
#include <bento4/Core/Ap4StszAtom.h>
#include <bento4/Core/Ap4StcoAtom.h>
#include <bento4/Core/Ap4Utils.h>

namespace ppbox
{
    namespace demux
    {

        class SampleListItem
            : public OrderedUnidirListHook<SampleListItem>::type
            , public AP4_Sample
        {
        public:
            SampleListItem()
                : itrack(size_t(-1))
            {
            }

        public:
            size_t itrack;
            boost::uint64_t time; // 毫秒
        };

        struct SampleOffsetLess
        {
            bool operator()(
                SampleListItem const & l, 
                SampleListItem const & r)
            {
                return l.GetOffset() < r.GetOffset();
            }
        };

        struct SampleTimeLess
        {
            bool operator()(
                SampleListItem const & l, 
                SampleListItem const & r)
            {
                return l.time < r.time 
                    || (l.time == r.time && l.itrack < r.itrack);
            }
        };

        class Track
            : public StreamInfo
        {
        public:
            Track(
                size_t itrack, 
                AP4_Track * track, 
                boost::uint64_t head_size, 
                error_code & ec)
                : track_(track)
                , next_index_(0)
                , chunk_index_(1)
            {
                if (track->GetType() != AP4_Track::TYPE_AUDIO
                    && track->GetType() != AP4_Track::TYPE_VIDEO) {
                        ec = bad_media_type;
                        return;
                }

                sample_.itrack = itrack;
                total_index_ = track_->GetSampleCount();

                if (total_index_ > 0) { 
                    if (!AP4_SUCCEEDED(GetNextSample()) || sample_.GetDescriptionIndex() == 0xFFFFFFFF) {
                        ec = bad_file_format;
                        return;
                    }
                }

                // Add
                char const * strStscPath = "mdia/minf/stbl/stsc";
                char const * strStszPath = "mdia/minf/stbl/stsz";
                char const * strStcoPath = "mdia/minf/stbl/stco";
                stco_ = (AP4_StcoAtom * )FindTrackChild(track_, strStcoPath);
                stsc_ = (AP4_StscAtom * )FindTrackChild(track_, strStscPath);
                stsz_ = (AP4_StszAtom * )FindTrackChild(track_, strStszPath);
                chunk_sum_ = stco_->GetChunkCount();

                GetMediaInfo(ec);
            }

            AP4_Track * operator->() const
            {
                return track_;
            }

            AP4_Result GetSample(
                AP4_Ordinal index, 
                SampleListItem & sample)
            {
                assert(index <= total_index_);
                if (index == total_index_) {
                    return AP4_ERROR_OUT_OF_RANGE;
                } else {
                    AP4_Result ret = track_->GetSample(index, sample);
                    // 正常情况是不会出错的，但是发现一部分影片的sample_count有问题
                    //assert(AP4_SUCCEEDED(ret));
                    return ret;
                }
            }

            AP4_Result GetNextSample()
            {
                assert(next_index_ <= total_index_);
                if (next_index_ == total_index_) {
                    return AP4_ERROR_OUT_OF_RANGE;
                } else {
                    AP4_Result ret = track_->GetSample(next_index_, sample_);
                    ++next_index_;
                    // 正常情况是不会出错的，但是发现一部分影片的sample_count有问题
                    //assert(AP4_SUCCEEDED(ret));
                    if (AP4_FAILED(ret)) {
                        total_index_ = next_index_;
                    }
                    return ret;
                }
            }

            AP4_Result Seek(
                AP4_UI64 & time, // dts
                AP4_Ordinal & next_index, 
                SampleListItem & sample)
            {
                AP4_Result ret = track_->GetSampleTable()->GetSampleIndexForTimeStamp(time, next_index);
                if (AP4_SUCCEEDED(ret)) {
                    next_index = track_->GetNearestSyncSampleIndex(next_index);
                    ret = track_->GetSample(next_index, sample);
                    assert(AP4_SUCCEEDED(ret));
                    ++next_index;
                    time = sample.GetDts();
                } else {
                    time = track_->GetDuration();
                    next_index = track_->GetSampleCount();
                }
                return ret;
            }

            AP4_Result Seek(
                AP4_UI64 & time, 
                AP4_Position & offset)
            {
                AP4_Result ret = Seek(time, next_index_, sample_);
                if (AP4_SUCCEEDED(ret))
                    offset = sample_.GetOffset();
                return ret;
            }

            AP4_Result Rewind()
            {
                next_index_ = 0;
                return AP4_SUCCESS;
            }

            // OLD
            AP4_Result GetBufferTime_tmp(
                AP4_Position offset, 
                AP4_UI32 time_hint, 
                AP4_UI32 & time)
            {
                AP4_Ordinal index = 0;
                if (AP4_FAILED(track_->GetSampleIndexForTimeStampMs(time_hint, index))) {
                    index = track_->GetSampleCount() - 1;
                }
                AP4_Sample ap4_sample;
                if (AP4_SUCCEEDED(track_->GetSample(index, ap4_sample))) {
                    if (ap4_sample.GetOffset() + ap4_sample.GetSize() > offset) {
                        while (index && 
                            AP4_SUCCEEDED(track_->GetSample(--index, ap4_sample)) && 
                            ap4_sample.GetOffset() + ap4_sample.GetSize() > offset);
                        if (ap4_sample.GetOffset() + ap4_sample.GetSize() <= offset)
                            time = (boost::uint32_t)(
                            ap4_sample.GetDts() * 1000 / track_->GetMediaTimeScale());
                        else
                            time = 0;
                    } else {
                        while (AP4_SUCCEEDED(track_->GetSample(++index, ap4_sample)) && 
                            ap4_sample.GetOffset() + ap4_sample.GetSize() < offset);
                        track_->GetSample(--index, ap4_sample);
                        time = (boost::uint32_t)(
                            ap4_sample.GetDts() * 1000 / track_->GetMediaTimeScale());
                    }
                } else {
                    time = 0;
                }
                return AP4_SUCCESS;
            }

            // New
            AP4_Result GetBufferTime(
                AP4_Position offset, 
                AP4_UI64 & time)
            {
                // 获取当前chunk_index的offset
                AP4_UI32 cur_offset;
                if (AP4_SUCCEEDED(stco_->GetChunkOffset(chunk_index_, cur_offset))) {
                    // 找到offset所在的chunk index 和这个块的开起位置
                    if (offset < cur_offset ) {
                        for (AP4_Ordinal index = chunk_index_ - 1; index > 0; --index) {
                            if (AP4_SUCCEEDED(stco_->GetChunkOffset(index, cur_offset))){
                                if (offset >= cur_offset) {
                                    chunk_index_ = index;
                                    break;
                                }
                            } else {
                                time = 0;
                                return AP4_SUCCESS;
                            }
                        }
                        if (offset < cur_offset ) {
                            time = 0;
                            return AP4_SUCCESS;
                        }
                    } else if (offset > cur_offset) {
                        for (AP4_Ordinal index = chunk_index_+1; index <= chunk_sum_; ++index) {
                            if (AP4_SUCCEEDED(stco_->GetChunkOffset(index, cur_offset))){
                                if (offset < cur_offset) {
                                    chunk_index_ = index - 1;
                                    if (AP4_FAILED(stco_->GetChunkOffset(chunk_index_, cur_offset))) {
                                        time = 0;
                                        return AP4_SUCCESS;
                                    }
                                    break;
                                } else if (offset == cur_offset){
                                     chunk_index_ = index;
                                }
                            } else {
                                time = 0;
                                return AP4_SUCCESS;
                            }
                        }
                    }
                    // 找到offset对应的sample index
                    AP4_Position left_offset = offset - cur_offset;
                    AP4_Ordinal begin_sample_index = 0;
                    AP4_Ordinal end_sample_index = 0;
                    AP4_Size sample_size = 0;
                    AP4_Ordinal sample_index = 0;
                    AP4_Size sum_sample_size = 0;
                    if (AP4_SUCCEEDED(stsc_->GetChunkSampleIndexs(chunk_index_, begin_sample_index, end_sample_index))) {
                        for (AP4_Ordinal i = begin_sample_index; i <= end_sample_index; ++i) {
                            if (AP4_SUCCEEDED(stsz_->GetSampleSize(i, sample_size))) {
                                sum_sample_size += sample_size;
                                if (sum_sample_size >= left_offset) {
                                    sample_index = i;
                                    break;
                                }
                            }
                        }

                        if (0 == sample_index) {
                            sample_index = end_sample_index + 1;
                        } else {
                            ++sample_index;
                        }
                        AP4_Sample ap4_sample;
                        if (AP4_SUCCEEDED(track_->GetSample(sample_index, ap4_sample))) {
                            time = ap4_sample.GetDts();
                        } else {
                            time = 0;
                        }
                    } else {
                        time = 0;
                    }
                } else {
                    time = 0;
                }
                return AP4_SUCCESS;
            }

        private:
            void GetMediaInfo(
                error_code & ec)
            {
                StreamInfo & media_info = *this;
                media_info.time_scale = track_->GetMediaTimeScale();
                media_info.start_time = 0;
                media_info.duration = track_->GetMediaDuration();
                ec = bad_file_format;
                if (AP4_Atom* avc1Atom = track_->GetTrakAtom()->FindChild("mdia/minf/stbl/stsd/avc1")) {
                    AP4_Avc1SampleEntry* avc1 = static_cast<AP4_Avc1SampleEntry*>(avc1Atom);
                    AP4_AvccAtom* avcc = AP4_DYNAMIC_CAST(AP4_AvccAtom, avc1->GetChild(AP4_ATOM_TYPE_AVCC));
                    if (avcc) {
                        media_info.type = StreamType::VIDE;
                        media_info.sub_type = VideoSubType::AVC1;
                        media_info.format_type = FormatType::video_avc_packet;
                        media_info.video_format.width = avc1->GetWidth();
                        media_info.video_format.height = avc1->GetHeight();
                        media_info.video_format.frame_rate = track_->GetSampleCount() * 1000 / track_->GetDurationMs();
                        const AP4_DataBuffer* di = &avcc->GetRawBytes();
                        if (di) {
                            AP4_Byte const * data = di->GetData();
                            AP4_Size size = di->GetDataSize();
                            AP4_Byte const * src = data + 5;
                            AP4_Byte const * src_end = data + size;
                            for (int i = 0; i < 2; i++) {
                                for( int n = *src++ & 0x1f; n > 0; n--) {
                                    int len = ((src[0] << 8) | src[1]) + 2;
                                    src += len;
                                    if(src > src_end) {
                                        break;
                                    }
                                }
                                if(src > src_end) {
                                    break;
                                }
                            }
                            if (src <= src_end) {
                                media_info.format_data.assign(data, src);
                                ec = error_code();
                            }
                        }
                    }
                } else if (AP4_SampleDescription* desc = track_->GetSampleDescription(sample_.GetDescriptionIndex())) {
                    AP4_MpegSampleDescription* mpeg_desc = NULL;
                    if(desc->GetType() == AP4_SampleDescription::TYPE_MPEG) {
                        mpeg_desc = AP4_DYNAMIC_CAST(AP4_MpegSampleDescription, desc);
                    } else if(desc->GetType() == AP4_SampleDescription::TYPE_PROTECTED) {
                        AP4_ProtectedSampleDescription* isma_desc = AP4_DYNAMIC_CAST(AP4_ProtectedSampleDescription, desc);
                        mpeg_desc = AP4_DYNAMIC_CAST(AP4_MpegSampleDescription, isma_desc->GetOriginalSampleDescription());
                    }
                    if (AP4_MpegVideoSampleDescription* video_desc = AP4_DYNAMIC_CAST(AP4_MpegVideoSampleDescription, mpeg_desc)) {
                        media_info.type = StreamType::VIDE;
                        media_info.video_format.width = video_desc->GetWidth();
                        media_info.video_format.height = video_desc->GetHeight();
                        media_info.video_format.frame_rate = track_->GetSampleCount()*1000/track_->GetDurationMs();
                        switch(video_desc->GetObjectTypeId()) {
                            case AP4_OTI_MPEG4_VISUAL:
                                media_info.sub_type = VideoSubType::MP4V;
                                media_info.format_type = FormatType::none;
                                {
                                    const AP4_DataBuffer & di = video_desc->GetDecoderInfo();
                                    AP4_Byte const * data = di.GetData();
                                    AP4_Size size = di.GetDataSize();
                                    media_info.format_data.assign(data, data + size);
                                }
                                ec = error_code();
                                break;
                            default:
                                break;
                        }
                    } else if (AP4_MpegAudioSampleDescription* audio_desc = AP4_DYNAMIC_CAST(AP4_MpegAudioSampleDescription, mpeg_desc)) {
                        media_info.type = StreamType::AUDI;
                        media_info.audio_format.sample_rate = audio_desc->GetSampleRate();
                        media_info.audio_format.sample_size = audio_desc->GetSampleSize();
                        media_info.audio_format.channel_count = audio_desc->GetChannelCount();
                        switch(audio_desc->GetObjectTypeId()) {
                            case AP4_OTI_MPEG4_AUDIO:
                            case AP4_OTI_MPEG2_AAC_AUDIO_MAIN: // ???
                            case AP4_OTI_MPEG2_AAC_AUDIO_LC: // ???
                            case AP4_OTI_MPEG2_AAC_AUDIO_SSRP: // ???
                                media_info.sub_type = AudioSubType::MP4A;
                                media_info.format_type = FormatType::audio_raw;
                                {
                                    const AP4_DataBuffer & di = audio_desc->GetDecoderInfo();
                                    AP4_Byte const * data = di.GetData();
                                    AP4_Size size = di.GetDataSize();
                                    media_info.format_data.assign(data, data + size);
                                }
                                ec = error_code();
                                break;
                            case AP4_OTI_MPEG1_AUDIO: // mp3
                                media_info.sub_type = AudioSubType::MP1A;
                                media_info.format_type = FormatType::audio_raw;
                                {
                                    const AP4_DataBuffer & di = audio_desc->GetDecoderInfo();
                                    AP4_Byte const * data = di.GetData();
                                    AP4_Size size = di.GetDataSize();
                                    media_info.format_data.assign(data, data + size);
                                }
                                ec = error_code();
                                break;
                            default:
                                media_info.sub_type = AudioSubType::NONE;
                                media_info.format_type = FormatType::none;
                                break;
                        }
                    }
                    //} else if(AP4_StsdAtom* stsd = AP4_DYNAMIC_CAST(AP4_StsdAtom, track->GetTrakAtom()->FindChild("mdia/minf/stbl/stsd"))) {
                }
            }

            AP4_Atom* FindTrackChild(AP4_Track * track, char const * path)
            {
                return track->GetTrakAtom()->FindChild(path);
            }

        public:
            //private:
            AP4_Track * track_;
            SampleListItem sample_;
            AP4_Ordinal next_index_;
            AP4_Ordinal total_index_;

            // ADD
            AP4_StcoAtom * stco_;
            AP4_StscAtom * stsc_;
            AP4_StszAtom * stsz_;
            AP4_Ordinal chunk_index_;
            AP4_Ordinal chunk_sum_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_TRACK_H_

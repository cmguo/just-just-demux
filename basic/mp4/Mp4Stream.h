// Mp4Stream.h

#ifndef _PPBOX_DEMUX_BASIC_MP4_MP4_STREAM_H_
#define _PPBOX_DEMUX_BASIC_MP4_MP4_STREAM_H_

#include <ppbox/avformat/Format.h>
#include <ppbox/avformat/mp4/lib/Mp4Track.h>
#include <ppbox/avformat/mp4/box/Mp4BoxEnum.h>

#include <ppbox/avcodec/CodecType.h>
#include <ppbox/avcodec/avc/AvcFormatType.h>
#include <ppbox/avcodec/aac/AacFormatType.h>

#include <framework/container/OrderedUnidirList.h>

namespace ppbox
{
    namespace demux
    {

        class Mp4Stream
            : public StreamInfo
            , public framework::container::OrderedUnidirListHook<Mp4Stream>::type
        {
        public:
            Mp4Stream(
                size_t itrack, 
                ppbox::avformat::Mp4Track & track, 
                TimestampHelper & helper)
                : track_(track)
                , helper_(helper)
                , samples_(track.sample_table())
                , time_(0)
                , offset_(0)
            {
                index = (boost::uint32_t)itrack;
                time_scale = track_.timescale();
                start_time = 0;
                duration = track_.duration();
                sample_count_ = samples_.count();
                if (!helper_.empty())
                    update();
            }

        public:
            boost::uint64_t time() const
            {
                return time_;
            }

            boost::uint64_t offset() const
            {
                return offset_;
            }

        public:
            bool seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec)
            {
                return samples_.seek(time, ec) && update();
            }

            bool next_sample(
                boost::system::error_code & ec)
            {
                return samples_.next(ec) && update();
            }

            bool limit(
                boost::uint64_t offset, 
                boost::uint64_t & time, 
                boost::system::error_code & ec)
            {
                return samples_.limit(offset, time, ec);
            }

            void get_sample(
                Sample & sample)
            {
                samples_.get(sample);
            }

        public:
            bool parse(
                boost::system::error_code & ec)
            {
                using namespace ppbox::avformat;

                ec.clear();

                Mp4SampleEntry const * entry(track_.sample_table().description());
                Mp4Box const * box = NULL;

                if (track_.type() == Mp4HandlerType::vide) {
                    type = StreamType::VIDE;
                    Mp4VisualSampleEntry const & video(static_cast<Mp4VisualSampleEntry const &>(*entry));
                    video_format.width = video.width();
                    video_format.height = video.height();
                    video_format.frame_rate(sample_count_ * time_scale, (boost::uint32_t)duration);
                    box = &video.box();
                } else {
                    type = StreamType::AUDI;
                    Mp4AudioSampleEntry const & audio(static_cast<Mp4AudioSampleEntry const &>(*entry));
                    audio_format.sample_rate = audio.sample_rate();
                    audio_format.sample_size = audio.sample_size();
                    audio_format.channel_count = audio.channel_count();
                    box = &audio.box();
                }

                if (Mp4EsDescription const * es_desc = entry->es_description()) {
                    if (es_desc->decoder_info())
                        format_data = es_desc->decoder_info()->Info;
                    boost::uint8_t object_type = es_desc->decoder_config()->ObjectTypeIndication;
                    context = (void const *)object_type;
                } else {
                    Mp4BoxVector const & vec = entry->vector();
                    if (!vec.empty()) {
                        Mp4Box & config(*vec.front());
                        Mp4Box::raw_data_t raw_data = config.raw_data();
                        format_data.assign(raw_data.address(), raw_data.address() + raw_data.size());
                        boost::uint32_t type = config.type;
                        context = (void const *)type;
                    }
                }

                Format::finish_from_stream(*this, "mp4", box->type, ec);

                return !ec;
            }

        private:
            bool update()
            {
                time_ = helper_.const_adjust(index, samples_.dts());
                offset_ = samples_.offset();
                return true;
            }

        public:
            struct StreamOffsetLess
            {
                bool operator()(
                    Mp4Stream const & l, 
                    Mp4Stream const & r)
                {
                    return l.offset_ < r.offset_;
                }
            };

            struct StreamTimeLess
            {
                bool operator()(
                    Mp4Stream const & l, 
                    Mp4Stream const & r)
                {
                    return l.time_ < r.time_ 
                        || (l.time_ == r.time_ && l.index < r.index);
                }
            };

            friend struct StreamOffsetLess;
            friend struct StreamTimeLess;

        public:
            typedef framework::container::OrderedUnidirList<
                Mp4Stream, 
                framework::container::identity<Mp4Stream>, 
                StreamOffsetLess
            > StreamOffsetList;

            typedef framework::container::OrderedUnidirList<
                Mp4Stream, 
                framework::container::identity<Mp4Stream>, 
                StreamTimeLess
            > StreamTimeList;

        private:
            ppbox::avformat::Mp4Track & track_;
            TimestampHelper & helper_;
            ppbox::avformat::Mp4SampleTable & samples_;
            boost::uint32_t sample_count_;
            boost::uint64_t time_; // ����
            boost::uint64_t offset_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_STREAM_H_

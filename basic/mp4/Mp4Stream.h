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
                : itrack_(itrack)
                , track_(track)
                , helper_(helper)
                , samples_(track.sample_table())
                , time_(0)
                , offset_(0)
            {
                time_scale = track_.timescale();
                start_time = 0;
                duration = track_.duration();
                sample_count_ = samples_.count();
                if (!helper_.empty())
                    update();
            }

        public:
            size_t itrack() const
            {
                return itrack_;
            }

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

                Mp4SampleEntry const & entry(track_.sample_table().description());

                if (track_.type() == Mp4HandlerType::vide) {
                    type = StreamType::VIDE;
                    Mp4VisualSampleEntry const & video(static_cast<Mp4VisualSampleEntry const &>(entry));
                    video_format.width = video.width();
                    video_format.height = video.height();
                    video_format.frame_rate(sample_count_ * time_scale, (boost::uint32_t)duration);
                } else {
                    type = StreamType::AUDI;
                    Mp4AudioSampleEntry const & audio(static_cast<Mp4AudioSampleEntry const &>(entry));
                    audio_format.sample_rate = audio.sample_rate();
                    audio_format.sample_size = audio.sample_size();
                    audio_format.channel_count = audio.channel_count();
                }

                Mp4EsDescription const * es_desc = entry.es_description();
                if (es_desc) {
                    if (es_desc->decoder_info())
                        format_data = es_desc->decoder_info()->Info;
                    boost::uint8_t object_type = es_desc->decoder_config()->ObjectTypeIndication;
                    Format::finish_from_stream(*this, "mp4", object_type, ec);
                } else if (Mp4Box const * avcC = entry.find_item("/avcC")) {
                    Mp4Box::raw_data_t raw_data = avcC->raw_data();
                    format_data.assign(raw_data.address(), raw_data.address() + raw_data.size());
                    Format::finish_from_stream(*this, "mp4", MpegObjectType::MPEG4_PART10_VISUAL, ec);
                } else {
                    ec = ppbox::avformat::error::bad_media_format;
                }

                return !ec;
            }

        private:
            bool update()
            {
                time_ = helper_.const_adjust(itrack_, samples_.dts());
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
                        || (l.time_ == r.time_ && l.itrack_ < r.itrack_);
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
            size_t itrack_;
            ppbox::avformat::Mp4Track & track_;
            TimestampHelper & helper_;
            ppbox::avformat::Mp4SampleTable & samples_;
            boost::uint32_t sample_count_;
            boost::uint64_t time_; // ∫¡√Î
            boost::uint64_t offset_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_STREAM_H_

// AviStream.h

#ifndef _PPBOX_DEMUX_BASIC_AVI_AVI_STREAM_H_
#define _PPBOX_DEMUX_BASIC_AVI_AVI_STREAM_H_

#include <ppbox/avformat/Format.h>
#include <ppbox/avformat/avi/AviFormat.h>
#include <ppbox/avformat/avi/lib/AviStream.h>
#include <ppbox/avformat/avi/box/AviBoxEnum.h>

#include <ppbox/avcodec/CodecType.h>
#include <ppbox/avcodec/avc/AvcFormatType.h>
#include <ppbox/avcodec/aac/AacFormatType.h>

#include <framework/container/OrderedUnidirList.h>

namespace ppbox
{
    namespace demux
    {

        class AviStream
            : public StreamInfo
            , public framework::container::OrderedUnidirListHook<AviStream>::type
        {
        public:
            AviStream(
                size_t istream, 
                ppbox::avformat::AviStream & stream, 
                TimestampHelper & helper)
                : stream_(stream)
                , helper_(helper)
                , time_(0)
                , offset_(0)
            {
                index = (boost::uint32_t)istream;
                time_scale = stream_.timescale();
                start_time = 0;
                duration = stream_.duration();
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
                return stream_.seek(time, ec) && update();
            }

            bool next_sample(
                boost::system::error_code & ec)
            {
                return stream_.next(ec) && update();
            }

            bool limit(
                boost::uint64_t offset, 
                boost::uint64_t & time, 
                boost::system::error_code & ec)
            {
                return stream_.limit(offset, time, ec);
            }

            void get_sample(
                Sample & sample)
            {
                sample = sample_;
            }

        public:
            bool parse(
                boost::system::error_code & ec)
            {
                using namespace ppbox::avformat;

                ec.clear();

                if (stream_.type() == AviStreamType::vids) {
                    type = StreamType::VIDE;
                    AviStreamFormatBox::VideoFormat const & video(stream_.video());
                    sub_type = video.biCompression;
                    video_format.width = video.biWidth;
                    video_format.height = video.biHeight;
                    video_format.frame_rate(stream_.timescale(), stream_.sample_duration());
                } else if (stream_.type() == AviStreamType::auds) {
                    type = StreamType::AUDI;
                    AviStreamFormatBox::AudioFormat const & audio(stream_.audio());
                    sub_type = audio.wFormatTag;
                    audio_format.sample_rate = audio.nSamplesPerSec;
                    audio_format.sample_size = audio.wBitsPerSample;
                    audio_format.channel_count = audio.nChannels;
                    format_data = audio.cbData;
                }

                Format::finish_from_stream(*this, "avi", sub_type, ec);

                return !ec;
            }

        private:
            bool update()
            {
                stream_.get(sample_);
                time_ = helper_.const_adjust(index, sample_.dts);
                offset_ = sample_.time;
                return true;
            }

        public:
            struct StreamOffsetLess
            {
                bool operator()(
                    AviStream const & l, 
                    AviStream const & r)
                {
                    return l.offset_ < r.offset_;
                }
            };

            struct StreamTimeLess
            {
                bool operator()(
                    AviStream const & l, 
                    AviStream const & r)
                {
                    return l.time_ < r.time_ 
                        || (l.time_ == r.time_ && l.index < r.index);
                }
            };

            friend struct StreamOffsetLess;
            friend struct StreamTimeLess;

        public:
            typedef framework::container::OrderedUnidirList<
                AviStream, 
                framework::container::identity<AviStream>, 
                StreamOffsetLess
            > StreamOffsetList;

            typedef framework::container::OrderedUnidirList<
                AviStream, 
                framework::container::identity<AviStream>, 
                StreamTimeLess
            > StreamTimeList;

        private:
            ppbox::avformat::AviStream & stream_;
            TimestampHelper & helper_;
            Sample sample_;
            boost::uint64_t time_; // ∫¡√Î
            boost::uint64_t offset_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_AVI_AVI_STREAM_H_

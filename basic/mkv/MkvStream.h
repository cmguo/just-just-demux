// MkvStream.h

#ifndef _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_
#define _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_

#include "ppbox/demux/base/DemuxBase.h"

#include <ppbox/avformat/mkv/MkvObjectType.h>
#include <ppbox/avformat/Format.h>

namespace ppbox
{
    namespace demux
    {

        class MkvStream
            : public ppbox::avformat::MkvTrackEntryData
            , public StreamInfo
        {
        public:
            MkvStream()
                : time_code_scale_(1)
                , dts_orgin_(0)
                , dts_(0)
            {
                index = (size_t)-1;
            }

            MkvStream(
                ppbox::avformat::MkvSegmentInfo file_prop, 
                ppbox::avformat::MkvTrackEntryData const & track)
                : ppbox::avformat::MkvTrackEntryData(track)
                , dts_orgin_(0)
                , dts_(0)
            {
                index = (size_t)-1;
                time_code_scale_ = (boost::uint32_t)file_prop.Time_Code_Scale.value();
                parse();
            }

        public:
            bool has_dts() const
            {
                return !DefaultDuration.empty();
            }

            boost::uint64_t dts() const
            {
                return dts_;
            }

            boost::uint32_t sample_duration() const
            {
                return (boost::uint32_t)(DefaultDuration.value() / time_code_scale_);
            }

            void next()
            {
                dts_orgin_ += DefaultDuration.value();
                dts_ = dts_orgin_ / time_code_scale_;
            }

            void set_start_time(
                boost::uint64_t dts)
            {
                dts_orgin_ = dts * time_code_scale_;
                dts_ = dts;
            }

        private:
            void parse()
            {
                using namespace ppbox::avformat;
                boost::system::error_code ec;

                time_scale = (boost::uint32_t)(1000000000 / time_code_scale_);
                format_data = CodecPrivate.value();
                context = CodecID.value().c_str();
                if (TrackType == MkvTrackType::VIDEO) { 
                    type = StreamType::VIDE;
                    video_format.width = (boost::uint32_t)Video.PixelWidth.value();
                    video_format.height = (boost::uint32_t)Video.PixelHeight.value();
                    if (DefaultDuration.empty()) {
                        video_format.frame_rate(0);
                    } else {
                        video_format.frame_rate(1000000000, (boost::uint32_t)DefaultDuration.value());
                    }
                    Format::finish_from_stream(*this, "mkv", 0, ec);
                } else if (TrackType == MkvTrackType::AUDIO) {
                    type = StreamType::AUDI;
                    audio_format.channel_count = (boost::uint32_t)Audio.Channels.value();
                    audio_format.sample_rate = (boost::uint32_t)(float)Audio.SamplingFrequency.value().as_int32();
                    audio_format.sample_size = (boost::uint32_t)Audio.BitDepth.value();
                    Format::finish_from_stream(*this, "mkv", 0, ec);
                } else if (TrackType == MkvTrackType::SUBTITLE) {
                    type = StreamType::SUBS;
                    Format::finish_from_stream(*this, "mkv", 0, ec);
                }
            }

        private:
            boost::uint32_t time_code_scale_;
            boost::uint64_t dts_orgin_;
            boost::uint64_t dts_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_

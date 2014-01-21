// MkvStream.h

#ifndef _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_
#define _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_

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
            {
                index = (size_t)-1;
            }

            MkvStream(
                ppbox::avformat::MkvTrackEntryData const & track)
                : ppbox::avformat::MkvTrackEntryData(track)
            {
                index = (size_t)-1;
                parse();
            }
/*
            void get_start_sample(
                std::vector<Sample> & samples)
            {
                if (sub_type == VideoSubType::AVC1) {
                    Sample sample;
                    sample.itrack = index_to_map;
                    sample.time = 0;
                    sample.ustime = 0;
                    sample.offset = 0;
                    sample.size = format_data.size();
                    sample.duration = 0;
                    sample.idesc = 0;
                    sample.dts = 0;
                    sample.cts_delta = 0;
                    sample.is_sync = false;
                    sample.is_discontinuity = false;
                    sample.data.swap(format_data);
                    samples.push_back(sample);
                }
            }
*/
        private:
            void parse()
            {
                using namespace ppbox::avformat;
                boost::system::error_code ec;

                format_data = CodecPrivate.value();
                context = CodecID.value().c_str();
                if (TrackType == MkvTrackType::VIDEO) { 
                    type = StreamType::VIDE;
                    video_format.width = (boost::uint32_t)Video.PixelWidth.value();
                    video_format.height = (boost::uint32_t)Video.PixelHeight.value();
                    video_format.frame_rate(0);
                    Format::finish_from_stream(*this, "mkv", 0, ec);
                } else if (TrackType == MkvTrackType::AUDIO) {
                    type = StreamType::AUDI;
                    audio_format.channel_count = (boost::uint32_t)Audio.Channels.value();
                    audio_format.sample_rate = (boost::uint32_t)(float)Audio.SamplingFrequency.value().as_int32();
                    audio_format.sample_size = (boost::uint32_t)Audio.BitDepth.value();
                    Format::finish_from_stream(*this, "mkv", 0, ec);
                }
            }
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MKV_MKV_STREAM_H_

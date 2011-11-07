// FlvDemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"
using namespace ppbox::demux::error;

#include <framework/logger/LoggerStreamRecord.h>
#include <framework/system/BytesOrder.h>
#include <framework/timer/TickCounter.h>
using namespace framework::logger;
using namespace framework::system;
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("FlvDemuxerBase", 0)

#include <iostream>

namespace ppbox
{
    namespace demux
    {

        error_code FlvDemuxerBase::open(
            error_code & ec)
        {
            open_step_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        bool FlvDemuxerBase::is_open(
            error_code & ec)
        {
            if (open_step_ == 3) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (boost::uint32_t)-1) {
                ec = error::not_open;
                return false;
            }

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                archive_ >> flv_header_;
                if (!archive_) {
                    archive_.clear();
                    ec = error::file_stream_error;
                    return false;
                }
                if (flv_header_.Signature1 != 'F'
                    || flv_header_.Signature2 != 'L'
                    || flv_header_.Signature3 != 'V') {
                        open_step_ = boost::uint32_t(-1);
                        ec = error::bad_file_format;
                        return false;
                }
                streams_.resize((size_t)TagType::FLV_TAG_TYPE_META);
                if (flv_header_.TypeFlagsAudio) {
                    stream_map_.push_back((size_t)TagType::FLV_TAG_TYPE_AUDIO);
                    streams_[(size_t)TagType::FLV_TAG_TYPE_AUDIO].index_to_map = stream_map_.size();
                }
                if (flv_header_.TypeFlagsVideo) {
                    stream_map_.push_back((size_t)TagType::FLV_TAG_TYPE_VIDEO);
                    streams_[(size_t)TagType::FLV_TAG_TYPE_VIDEO].index_to_map = stream_map_.size();
                }
                open_step_ = 1;
            }

            if (open_step_ == 1) {
                archive_.seekg(std::ios::off_type(flv_header_.DataOffset), std::ios_base::beg);
                while (!get_tag(flv_tag_, ec)) {
                    std::vector<boost::uint8_t> codec_data;
                    archive_.seekg(std::streamoff(flv_tag_.data_offset), std::ios_base::beg);
                    util::serialization::serialize_collection(archive_, codec_data, flv_tag_.DataSize);
                    boost::uint32_t index_to_map = streams_[(size_t)flv_tag_.Type].index_to_map;
                    streams_[(size_t)flv_tag_.Type] = FlvStream(flv_tag_, codec_data);
                    streams_[(size_t)flv_tag_.Type].index_to_map = index_to_map;
                    streams_[(size_t)flv_tag_.Type].ready = true;
                    bool ready = true;
                    for (size_t i = 0; i < stream_map_.size(); ++i) {
                        if (!streams_[stream_map_[i]].ready) {
                            ready = false;
                            break;
                        }
                    }
                    if (ready)
                        break;
                }
                if (ec) {
                    return false;
                }
                open_step_ = 2;
            }

            if (open_step_ == 2) {
                if (get_tag(flv_tag_, ec))
                    return ec;
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                timestamp_offset_ms_ = flv_tag_.Timestamp;
                open_step_ = 3;
            }

            return true;
        }

        error_code FlvDemuxerBase::get_tag(
            FlvTag & flv_tag_,
            error_code & ec)
        {
            archive_ >> flv_tag_;
            ec.clear();
            if (!archive_) {
                archive_.clear();
                ec = error::file_stream_error;
            } else {
                if (flv_tag_.DataSize+11 != flv_tag_.PreTagSize) {
                    archive_.clear();
                    ec = error::bad_file_format;
                }
            }
            return ec;
        }

        error_code FlvDemuxerBase::parse_stream(
            error_code &ec)
        {
            ec.clear();
            streams_[0].type = MEDIA_TYPE_AUDI;
            streams_[0].time_scale = 1000;
            if (metadata_.audiocodecid == SoundCodec::FLV_CODECID_AAC) {
                streams_[0].sub_type = AUDIO_TYPE_MP4A;
                streams_[0].format_type = MediaInfo::audio_iso_mp4;
                if (metadata_.stereo) {
                    streams_[0].audio_format.channel_count = 2;
                } else {
                    streams_[0].audio_format.channel_count = 1;
                }
                streams_[0].audio_format.sample_size = metadata_.audiosamplesize;
                streams_[0].audio_format.sample_rate = metadata_.audiosamplerate;
            } else {
                ec = error::bad_file_format;
            }

            streams_[1].type = MEDIA_TYPE_VIDE;
            streams_[0].time_scale = 1000;
            if (metadata_.videocodecid == VideoCodec::FLV_CODECID_H264) {
                streams_[1].sub_type = VIDEO_TYPE_AVC1;
                streams_[1].format_type = MediaInfo::video_avc_packet;
                streams_[1].video_format.frame_rate = metadata_.framerate;
                streams_[1].video_format.height = metadata_.height;
                streams_[1].video_format.width = metadata_.width;
            } else {
                ec = error::bad_file_format;
            }
            return ec;
        }

        error_code FlvDemuxerBase::close(error_code & ec)
        {
            ec.clear();
            open_step_ = boost::uint32_t(-1);
            return ec;
        }

        error_code FlvDemuxerBase::get_sample(
            Sample & sample, 
            boost::system::error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            FlvTag flv_tag_;
            archive_.seekg(parse_offset_, std::ios_base::beg);
            framework::timer::TickCounter tc;
            if (get_tag(flv_tag_, ec)) {
                return ec;
            }
            if (tc.elapsed() > 10) {
                LOG_S(Logger::kLevelDebug, "[get_tag], elapse " << tc.elapsed());
            }
            parse_offset_ = (boost::uint32_t)archive_.tellg();
            if (flv_tag_.Type < streams_.size() && 
                streams_[(size_t)flv_tag_.Type].index_to_map < stream_map_.size()) {
                    FlvStream const & stream = streams_[(size_t)flv_tag_.Type];
                    sample.itrack = stream.index_to_map;
                    sample.idesc = 0;
                    sample.flags = 0;
                    sample.time = flv_tag_.Timestamp - timestamp_offset_ms_;
                    sample.ustime = sample.time * 1000;
                    sample.dts = flv_tag_.Timestamp;
                    sample.pts = (boost::uint32_t)-1;
                    sample.is_sync = flv_tag_.Type == TagType::FLV_TAG_TYPE_AUDIO 
                        || flv_tag_.VideoTag.FrameType == FrameType::FLV_FRAME_KEY;
                    sample.size = flv_tag_.DataSize;
                    sample.blocks.push_back(FileBlock(flv_tag_.data_offset, flv_tag_.DataSize));
            } else {
                ec = bad_file_format;
            }
            return ec;
        }

        size_t FlvDemuxerBase::get_media_count(error_code & ec)
        {
            if (is_open(ec)) {
                return streams_.size();
            } else {
                return 0;
            }
        }

        error_code FlvDemuxerBase::get_media_info(
            size_t index, 
            MediaInfo & info, 
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                if (index >= streams_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_[index];
                }
            }
            return ec;
        }

        boost::uint32_t FlvDemuxerBase::get_duration(
            boost::system::error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }

        boost::uint32_t FlvDemuxerBase::get_end_time(
            boost::system::error_code & ec)
        {
            return 0;
        }

        boost::uint32_t FlvDemuxerBase::get_cur_time(
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                return flv_tag_.Timestamp - timestamp_offset_ms_;
            }
            return 0;
        }

        boost::uint64_t FlvDemuxerBase::seek_to(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }
    }
}

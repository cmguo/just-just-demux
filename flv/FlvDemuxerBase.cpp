// FlvDemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"
using namespace ppbox::demux::error;
using namespace ppbox::avformat;

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
            parse_offset_ = 0;
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

            assert(archive_);

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                archive_ >> flv_header_;
                if (archive_) {
                    streams_.clear();
                    stream_map_.clear();
                    streams_.resize((size_t)TagType::FLV_TAG_TYPE_META + 1);
                    if (flv_header_.TypeFlagsAudio) {
                        streams_[(size_t)TagType::FLV_TAG_TYPE_AUDIO].index = stream_map_.size();
                        stream_map_.push_back((size_t)TagType::FLV_TAG_TYPE_AUDIO);
                    }
                    if (flv_header_.TypeFlagsVideo) {
                        streams_[(size_t)TagType::FLV_TAG_TYPE_VIDEO].index = stream_map_.size();
                        stream_map_.push_back((size_t)TagType::FLV_TAG_TYPE_VIDEO);
                    }
                    open_step_ = 1;
                    parse_offset_ = std::ios::off_type(flv_header_.DataOffset) + 4; // + 4 PreTagSize
                } else {
                    if (archive_.failed()) {
                        ec = error::bad_file_format;
                    } else {
                        ec = error::file_stream_error;
                    }
                }
            }

            if (open_step_ == 1) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                while (!get_tag(flv_tag_, ec)) {
                    if (flv_tag_.Type == TagType::FLV_TAG_TYPE_META) {
                        parse_metadata(flv_tag_);
                    }
                    std::vector<boost::uint8_t> codec_data;
                    archive_.seekg(std::streamoff(flv_tag_.data_offset), std::ios_base::beg);
                    util::serialization::serialize_collection(archive_, codec_data, (boost::uint32_t)flv_tag_.DataSize);
                    archive_.seekg(4, std::ios_base::cur);
                    assert(archive_);
                    parse_offset_ = (boost::uint32_t)archive_.tellg();
                    boost::uint32_t index = streams_[(size_t)flv_tag_.Type].index;
                    streams_[(size_t)flv_tag_.Type] = FlvStream(flv_tag_, codec_data, metadata_);
                    streams_[(size_t)flv_tag_.Type].index = index;
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
                if (!ec) {
                    if (timestamp_offset_ms_ == 0) {
                        open_step_ = 2;
                    } else {
                        header_offset_ = parse_offset_;
                        open_step_ = 3;
                    }
                }
            }

            if (open_step_ == 2) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                while (!get_tag(flv_tag_, ec)) {
                    if (flv_tag_.Type < streams_.size() &&
                        streams_[(size_t)flv_tag_.Type].index < stream_map_.size()) {
                            break;
                    }
                }
                if (!ec) {
                    archive_.seekg(parse_offset_, std::ios_base::beg);
                    timestamp_offset_ms_ = flv_tag_.Timestamp;
                    header_offset_ = parse_offset_;
                    open_step_ = 3;
                }
            }

            if (ec) {
                archive_.clear();
                archive_.seekg(parse_offset_, std::ios_base::beg);
                return false;
            }

            return true;
        }

        error_code FlvDemuxerBase::get_tag(
            FlvTag & flv_tag,
            error_code & ec)
        {
            if (archive_ >> flv_tag) {
                ec.clear();
                return ec;
            } else if (archive_.failed()) {
                archive_.clear();
                return ec = error::bad_file_format;
            } else {
                archive_.clear();
                return ec = error::file_stream_error;
            }
        }

        //error_code FlvDemuxerBase::parse_stream(
        //    error_code &ec)
        //{
        //    ec.clear();
        //    streams_[0].type = MEDIA_TYPE_AUDI;
        //    streams_[0].time_scale = 1000;
        //    if (metadata_.audiocodecid == SoundCodec::FLV_CODECID_AAC) {
        //        streams_[0].sub_type = AUDIO_TYPE_MP4A;
        //        streams_[0].format_type = MediaInfo::audio_iso_mp4;
        //        if (metadata_.stereo) {
        //            streams_[0].audio_format.channel_count = 2;
        //        } else {
        //            streams_[0].audio_format.channel_count = 1;
        //        }
        //        streams_[0].audio_format.sample_size = metadata_.audiosamplesize;
        //        streams_[0].audio_format.sample_rate = metadata_.audiosamplerate;
        //    } else {
        //        ec = error::bad_file_format;
        //    }

        //    streams_[1].type = MEDIA_TYPE_VIDE;
        //    streams_[1].time_scale = 1000;
        //    if (metadata_.videocodecid == VideoCodec::FLV_CODECID_H264) {
        //        streams_[1].sub_type = VIDEO_TYPE_AVC1;
        //        streams_[1].format_type = MediaInfo::video_avc_packet;
        //        streams_[1].video_format.frame_rate = metadata_.framerate;
        //        streams_[1].video_format.height = metadata_.height;
        //        streams_[1].video_format.width = metadata_.width;
        //    } else {
        //        ec = error::bad_file_format;
        //    }
        //    return ec;
        //}

        void FlvDemuxerBase::parse_metadata(
            FlvTag const & metadata_tag)
        {
            std::vector<FlvDataObjectProperty> const & variables = 
            metadata_tag.DataTag.Value.ECMAArray.Variables;
            for (boost::uint32_t i = 0; i < variables.size(); ++i) {
                FlvDataObjectProperty const & property = variables[i];
                if (property.PropertyName.StringData == "width") {
                    metadata_.width = (double)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "height") {
                    metadata_.height = (double)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "framerate") {
                    metadata_.framerate = (double)property.PropertyData.Double;
                }
            }
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
            archive_.seekg(parse_offset_, std::ios_base::beg);
            assert(archive_);
            framework::timer::TickCounter tc;
            if (get_tag(flv_tag_, ec)) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                return ec;
            }
            if (tc.elapsed() > 10) {
                LOG_S(Logger::kLevelDebug, "[get_tag], elapse " << tc.elapsed());
            }
            parse_offset_ = (boost::uint32_t)archive_.tellg();
            if (flv_tag_.is_sample) {
                assert(flv_tag_.Type < streams_.size() && 
                    streams_[(size_t)flv_tag_.Type].index < stream_map_.size());
                FlvStream const & stream = streams_[(size_t)flv_tag_.Type];
                sample.itrack = stream.index;
                sample.idesc = 0;
                sample.flags = 0;
                if (flv_tag_.is_sync)
                    sample.flags |= Sample::sync;
                boost::uint64_t timestamp = timestamp_.transfer((boost::uint32_t)flv_tag_.Timestamp);
                sample.time = (boost::uint32_t)timestamp - timestamp_offset_ms_;
                sample.ustime = (timestamp - timestamp_offset_ms_) * 1000;
                sample.dts = timestamp - timestamp_offset_ms_;
                sample.cts_delta = flv_tag_.cts_delta;
                sample.us_delta = 1000*sample.cts_delta;
                sample.duration = 0;
                sample.size = flv_tag_.DataSize;
                sample.blocks.clear();
                sample.blocks.push_back(FileBlock(flv_tag_.data_offset, flv_tag_.DataSize));
            } else if (flv_tag_.Type == TagType::FLV_TAG_TYPE_META) {
                LOG_S(Logger::kLevelDebug, "[get_sample] script data: " << flv_tag_.DataTag.Name.String.StringData);
                return get_sample(sample, ec);
            } else if (flv_tag_.Type == TagType::FLV_TAG_TYPE_AUDIO) {
                if (flv_tag_.AudioHeader.SoundFormat == SoundCodec::FLV_CODECID_AAC
                    && flv_tag_.AudioHeader.AACPacketType == 0) {
                        LOG_S(Logger::kLevelDebug, "[get_sample] duplicate aac sequence header");
                        return get_sample(sample, ec);
                }
                ec = bad_file_format;
            } else if (flv_tag_.Type == TagType::FLV_TAG_TYPE_VIDEO) {
                if (flv_tag_.VideoHeader.CodecID == VideoCodec::FLV_CODECID_H264) {
                    if (flv_tag_.VideoHeader.AVCPacketType == 0) {
                        LOG_S(Logger::kLevelDebug, "[get_sample] duplicate aac sequence header");
                        return get_sample(sample, ec);
                    } else if (flv_tag_.VideoHeader.AVCPacketType == 2) {
                        LOG_S(Logger::kLevelDebug, "[get_sample] end of avc sequence");
                        return get_sample(sample, ec);
                    }
                } 
                ec = bad_file_format;
            } else {
                ec = bad_file_format;
            }
            return ec;
        }

        size_t FlvDemuxerBase::get_media_count(error_code & ec)
        {
            if (is_open(ec)) {
                return stream_map_.size();
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
                if (index >= stream_map_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_[stream_map_[index]];
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

        boost::uint64_t FlvDemuxerBase::seek(
            boost::uint32_t & time, 
            boost::system::error_code & ec)
        {
            if (time == 0) {
                ec.clear();
                parse_offset_ = header_offset_;
                return header_offset_;
            } else {
                ec = error::not_support;
                return 0;
            }
        }

        boost::uint64_t FlvDemuxerBase::get_offset(
            boost::uint32_t & time, 
            boost::uint32_t & delta, 
            boost::system::error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }
    }
}

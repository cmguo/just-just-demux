// FlvDemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/flv/FlvDemuxerBase.h"
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
        static boost::uint32_t GetSamplingFrequency(
            boost::uint8_t index)
        {
            switch (index) {
                case 0: return 5500;
                case 1: return 11025;
                case 2: return 22050;
                case 3: return 44100;
                default: return 44100;
            }
        }

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
            if (open_step_ == 2) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (boost::uint32_t)-1) {
                ec = error::not_open;
                return false;
            }

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                FlvHeader flv_header;
                archive_ >> flv_header;
                if (!archive_) {
                    archive_.clear();
                    ec = error::file_stream_error;
                    return false;
                }
                if (flv_header.Signature1 != 'F'
                    || flv_header.Signature2 != 'L'
                    || flv_header.Signature3 != 'V') {
                        open_step_ = boost::uint32_t(-1);
                        ec = error::bad_file_format;
                } else {
                    boost::uint32_t read_tag_number = 0;
                    while (read_tag_number < 3) {
                        read_tag_number++;
                        std::vector<boost::uint8_t> buf;
                        FlvTag flv_tag(buf);
                        if (get_tag(flv_tag, ec)) {
                            break;
                        }
                        if (flv_tag.TagType == TagType::FLV_TAG_TYPE_AUDIO) {
                            if (streams_info_.size() == 2 
                                && flv_tag.TagData.size() > 2
                                && flv_tag.TagData.at(1) == 0) {
                                parse_audio_config(flv_tag.TagData.at(0));
                                flv_tag.TagData.erase(flv_tag.TagData.begin(), flv_tag.TagData.begin()+2);
                                streams_info_[0].format_data.assign(flv_tag.TagData.begin(), flv_tag.TagData.end());
                            }
                        } else if (flv_tag.TagType == TagType::FLV_TAG_TYPE_META) {
                            metadata_.BinaryData = flv_tag.TagData;
                            if (parse_metadata(&metadata_.BinaryData.at(0), metadata_.BinaryData.size(), ec)) {
                                break;
                            }
                        } else if (flv_tag.TagType == TagType::FLV_TAG_TYPE_VIDEO) {
                            if (streams_info_.size() == 2 
                                && flv_tag.TagData.size() > 5
                                && flv_tag.TagData.at(4) == 0) {
                                flv_tag.TagData.erase(flv_tag.TagData.begin(), flv_tag.TagData.begin()+5);
                                streams_info_[1].format_data.assign(flv_tag.TagData.begin(), flv_tag.TagData.end());
                            }
                        }
                    }
                    if (ec) {
                        return false;
                    } else {
                        if (!parse_stream(ec)
                            && !streams_info_[0].format_data.empty()
                            && !streams_info_[1].format_data.empty()) {
                                header_offset_ = (boost::uint32_t)archive_.tellg();
                                open_step_ = 1;
                        } else {
                            ec = error::bad_file_format;
                            return false;
                        }
                    }
                }

                if (open_step_ == 1) {
                    archive_.seekg(header_offset_, std::ios_base::beg);
                    while(true) {
                        std::vector<boost::uint8_t> buf;
                        FlvTag flv_tag(buf);
                        if (get_tag(flv_tag, ec)) {
                            break;
                        }
                        if (flv_tag.TagType == TagType::FLV_TAG_TYPE_AUDIO
                            || flv_tag.TagType == TagType::FLV_TAG_TYPE_VIDEO) {
                                sample_.time = flv_tag.Timestamp;
                                sample_.ustime = flv_tag.Timestamp*1000;
                                if (!reopen_)
                                    timestamp_offset_ms_ = flv_tag.Timestamp;
                                break;
                        } else if (flv_tag.TagType == TagType::FLV_TAG_TYPE_META) {
                            error_code lec;
                            parse_metadata(&flv_tag.TagData.at(0), flv_tag.TagData.size(), lec);
                            header_offset_ = (boost::uint32_t)archive_.tellg();
                        } else {
                            ec = error::bad_file_format;
                        }
                    }
                    if (ec) {
                        return false;
                    } else {
                        parse_offset_ = header_offset_;
                        archive_.seekg(header_offset_, std::ios_base::beg);
                        open_step_ = 2;
                        reopen_ = true;
                        return true;
                    }
                }
            }
            return true;
        }

        void FlvDemuxerBase::parse_audio_config(
            boost::uint8_t audio_config)
        {
            boost::uint8_t sound_codec = (boost::uint8_t)(audio_config >> 4);
            boost::uint8_t sound_rate = (boost::uint8_t)((audio_config & 0x0C) >> 2);
            boost::uint8_t sound_size = (boost::uint8_t)((audio_config & 0x02) >> 1);
            boost::uint8_t sound_type = (boost::uint8_t)(audio_config & 0x01);
            metadata_.audiosamplerate = GetSamplingFrequency(sound_rate);
            metadata_.audiocodecid = sound_codec;
            metadata_.audiosamplesize = sound_size ? 16 : 8;
            metadata_.stereo = sound_type ? 1 : 0;
        }

        error_code FlvDemuxerBase::get_tag(
            FlvTag & flv_tag,
            error_code & ec)
        {
            archive_ >> flv_tag;
            ec.clear();
            if (!archive_) {
                archive_.clear();
                ec = error::file_stream_error;
            } else {
                if (flv_tag.DataSize+11 != flv_tag.PreTagSize) {
                    archive_.clear();
                    ec = error::bad_file_format;
                }
            }
            return ec;
        }

        error_code FlvDemuxerBase::parse_metadata(
            boost::uint8_t const *buffer,
            boost::uint32_t size,
            error_code & ec)
        {
            boost::uint32_t pos = 0;
            if (find_amf_array(buffer, size, pos)) {
                boost::uint32_t length = 0;
                parse_amf_array(buffer+pos, size-pos, length, ec);
            } else {
                ec = error::bad_file_format;
            }
            return ec;
        }

        error_code FlvDemuxerBase::parse_amf_array(
            boost::uint8_t const *buffer,
            boost::uint32_t size,
            boost::uint32_t & length,
            error_code & ec)
        {
            boost::uint8_t const *p = buffer;
            boost::uint32_t data_size = size;
            std::string str_value;
            bool bool_value;
            double double_value;
            boost::int32_t skip_size = 0;
            boost::uint32_t pos = 0;
            while (true) {
                skip_size = get_amf_string(p+pos, size-pos, str_value);
                if (skip_size >= 0) {
                    pos += skip_size;
                    pos++;
                    if (str_value == "author") {
                        skip_size = get_amf_string(p+pos, data_size-pos, str_value);
                        metadata_.author = str_value;
                    } else if (str_value == "copyright") {
                        skip_size = get_amf_string(p+pos, data_size-pos, str_value);
                        metadata_.copyright = str_value;
                    } else if (str_value == "description") {
                        skip_size = get_amf_string(p+pos, data_size-pos, str_value);
                        metadata_.description = str_value;
                    } else if (str_value == "duration") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.duration = (boost::uint32_t)double_value;
                    } else if (str_value == "datarate") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.datarate = (boost::uint32_t)double_value;
                    } else if (str_value == "livetime") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.livetime = (boost::uint64_t)double_value;
                    } else if (str_value == "timeshift") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.timeshift = (boost::uint64_t)double_value;
                    } else if (str_value == "width") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.width = (boost::uint32_t)double_value;
                    } else if (str_value == "height") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.height = (boost::uint32_t)double_value;
                    } else if (str_value == "videodatarate") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.videodatarate = (boost::uint32_t)double_value;
                    } else if (str_value == "framerate") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.framerate = (boost::uint32_t)double_value;
                    } else if (str_value == "videocodecid") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.videocodecid = (boost::uint32_t)double_value;
                    } else if (str_value == "audiosamplerate") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.audiosamplerate = (boost::uint32_t)double_value;
                    } else if (str_value == "audiosamplesize") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.audiosamplesize = (boost::uint32_t)double_value;
                    } else if (str_value == "stereo") {
                        skip_size = get_amf_bool(p+pos, data_size-pos, bool_value);
                        metadata_.stereo = bool_value;
                    } else if (str_value == "audiocodecid") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.audiocodecid = (boost::uint32_t)double_value;
                    } else if (str_value == "filesize") {
                        skip_size = get_amf_number(p+pos, data_size-pos, double_value);
                        metadata_.filesize = (boost::uint32_t)double_value;
                    } else {
                        pos--;
                        skip_size = get_amf_unknow(p+pos, data_size-pos);
                    }

                    if (skip_size >= 0) {
                        pos += skip_size;
                        if ((size-pos) >= 3) {
                            if (*(p+pos) == 0x00 && *(p+pos+1) == 0x00 && *(p+pos+2) == 0x09) {
                                // end amf array
                                pos = pos + 3;
                                length = pos;
                                break;
                            }
                        } else {
                            ec = error::bad_file_format;
                            break;
                        }
                    } else {
                        ec = error::bad_file_format;
                        break;
                    }
                } else {
                    ec = error::bad_file_format;
                    break;
                }
            }
            return ec;
        }

        error_code FlvDemuxerBase::parse_stream(
            error_code &ec)
        {
            ec.clear();
            streams_info_[0].type = MEDIA_TYPE_AUDI;
            streams_info_[0].time_scale = 1000;
            if (metadata_.audiocodecid == SoundCodec::FLV_CODECID_AAC) {
                streams_info_[0].sub_type = AUDIO_TYPE_MP4A;
                streams_info_[0].format_type = MediaInfo::audio_flv_tag;
                if (metadata_.stereo) {
                    streams_info_[0].audio_format.channel_count = 2;
                } else {
                    streams_info_[0].audio_format.channel_count = 1;
                }
                streams_info_[0].audio_format.sample_size = metadata_.audiosamplesize;
                streams_info_[0].audio_format.sample_rate = metadata_.audiosamplerate;
            } else {
                ec = error::bad_file_format;
            }

            streams_info_[1].type = MEDIA_TYPE_VIDE;
            streams_info_[0].time_scale = 1000;
            if (metadata_.videocodecid == VideoCodec::FLV_CODECID_H264) {
                streams_info_[1].sub_type = VIDEO_TYPE_AVC1;
                streams_info_[1].format_type = MediaInfo::video_flv_tag;
                streams_info_[1].video_format.frame_rate = metadata_.framerate;
                streams_info_[1].video_format.height = metadata_.height;
                streams_info_[1].video_format.width = metadata_.width;
            } else {
                ec = error::bad_file_format;
            }
            return ec;
        }

        boost::int32_t FlvDemuxerBase::get_amf_string(
            boost::uint8_t const * buffer,
            boost::uint32_t size,
            std::string & str)
        {
            boost::uint16_t   length;
            memcpy((boost::uint8_t*)&length, buffer, 2);
            length = BytesOrder::big_endian_to_host_short(length);
            std::string res;
            if ((boost::uint32_t)(length+2) <= size) {
                if (length == 0) {
                    str = "";
                } else {
                    res.resize(length);
                    memcpy(&res.at(0), buffer+2, length);
                    str = res;
                }
                return length + 2;
            } else {
                return boost::uint32_t(-1);
            }
        }

        boost::int32_t FlvDemuxerBase::get_amf_bool(
            boost::uint8_t const * buffer,
            boost::uint32_t size,
            bool & value)
        {
            if (size >= 1) {
                if (*buffer == 0) {
                    value = 0;
                } else {
                    value = 1;
                }
                return 1;
            } else {
                return boost::uint32_t(-1);
            }
        }

        boost::int32_t FlvDemuxerBase::get_amf_number(
            boost::uint8_t const * buffer,
            boost::uint32_t size,
            double & value)
        {
            if (size >= 8) {
                boost::uint64_t res;
                memcpy((boost::uint8_t*)&res, buffer, 8);
                res = BytesOrder::big_endian_to_host_longlong(res);
                union {
                    boost::uint8_t dc[8];
                    double dd;
                } d;
                memcpy(d.dc, (boost::uint8_t*)&res, 8);
                value = d.dd;
                return 8;
            } else {
                return boost::uint32_t(-1);
            }
        }

        boost::int32_t FlvDemuxerBase::get_amf_array(
            boost::uint8_t const *buffer,
            boost::uint32_t size,
            boost::uint32_t & array_size)
        {
            if (size >= 4) {
                boost::uint32_t res;
                memcpy((boost::uint8_t*)&res, buffer, 4);
                res = BytesOrder::big_endian_to_host_long(res);
                array_size = res;
                return 4;
            } else {
                return boost::uint32_t(-1);
            }
        }

        boost::int32_t FlvDemuxerBase::get_amf_unknow(
            boost::uint8_t const * buffer,
            boost::uint32_t size)
        {
            boost::uint8_t type = *buffer;
            boost::uint16_t length = 0;
            error_code ec;
            boost::uint32_t len = 0;
            switch(type) {
                case AMFDataType::AMF_DATA_TYPE_NUMBER:
                    return 8+1;
                case AMFDataType::AMF_DATA_TYPE_BOOL:
                    return 1+1;
                case AMFDataType::AMF_DATA_TYPE_STRING:
                    memcpy((boost::uint8_t*)&length, buffer+1, 2);
                    length = BytesOrder::big_endian_to_host_short(length);
                    return length+3;
                case AMFDataType::AMF_DATA_TYPE_MIXEDARRAY:
                    parse_amf_array(buffer, size, len, ec);
                    if (ec) {
                        return -1;
                    } else {
                        return len;
                    }
                default:
                    assert(0);
                    return 0;
            }
        }

        bool FlvDemuxerBase::find_amf_array(
            boost::uint8_t const *p,
            boost::uint32_t size,
            boost::uint32_t & pos)
        {
            std::string str_value;
            bool        bool_value;
            double      double_value;
            boost::uint32_t array_size;
            boost::uint32_t position = 0;
            boost::uint32_t ship_size = 0;
            while ( (size - position) > 3) {
                boost::uint8_t datatype = *(p+position);
                position++;
                switch(datatype) {
                    case AMFDataType::AMF_DATA_TYPE_STRING:
                        ship_size = get_amf_string(p+position, size-position, str_value);
                        if (ship_size >= 0) {
                            position += ship_size;
                        } else {
                            return false;
                        }
                        break;
                    case AMFDataType::AMF_DATA_TYPE_BOOL:
                        ship_size = get_amf_bool(p+position, size-position, bool_value);
                        if (ship_size >= 0) {
                            position += ship_size;
                        } else {
                            return false;
                        }
                        break;
                    case AMFDataType::AMF_DATA_TYPE_NUMBER:
                        ship_size = get_amf_number(p+position, size-position, double_value);
                        if (ship_size >= 0) {
                            position += ship_size;
                        } else {
                            return false;
                        }
                        break;
                    case AMFDataType::AMF_DATA_TYPE_MIXEDARRAY:
                        ship_size = get_amf_array(p+position, size-position, array_size);
                        if (ship_size >= 0) {
                            position += ship_size;
                            pos = position;
                            return true;
                        } else {
                            return false;
                        }
                        break;
                    default:
                        return false;
                        break;
                }
            }
            return false;
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
            FlvTag flv_tag(sample.data);
            archive_.seekg(parse_offset_, std::ios_base::beg);
            framework::timer::TickCounter tc;
            if (get_tag(flv_tag, ec)) {
                return ec;
            }
            if (tc.elapsed() > 10) {
                LOG_S(Logger::kLevelDebug, "[get_tag], elapse " << tc.elapsed());
            }
            parse_offset_ = (boost::uint32_t)archive_.tellg();
            sample_.time = flv_tag.Timestamp;
            sample_.offset = 0;
            sample_.duration = 0;
            sample_.idesc = 0;
            sample_.dts = flv_tag.Timestamp;
            sample_.cts_delta = 0;
            sample_.is_discontinuity = false;
            if (flv_tag.TagType == TagType::FLV_TAG_TYPE_AUDIO) {
                sample_.itrack = 0;
                sample_.is_sync = true;
                if (flv_tag.TagData.size() >= 2) {
                    flv_tag.TagData.erase(flv_tag.TagData.begin(), flv_tag.TagData.begin()+2);
                } else {
                    ec = error::bad_file_format;
                }
            } else if (flv_tag.TagType == TagType::FLV_TAG_TYPE_VIDEO) {
                sample_.itrack = 1;
                boost::uint32_t frame_type = (flv_tag.TagData.at(0) & 0xF0) >> 4;
                if (frame_type == 1) {
                    sample_.is_sync = true;
                } else {
                    sample_.is_sync = false;
                }
                if (flv_tag.TagData.size() >= 5) {
                    flv_tag.TagData.erase(flv_tag.TagData.begin(), flv_tag.TagData.begin()+5);
                } else {
                    ec = error::bad_file_format;
                }
            } else {
                sample_.itrack = boost::uint32_t(-1);
                sample_.is_sync = false;
            }
            sample.cts_delta = sample_.cts_delta;
            sample.dts = sample_.dts;
            sample.duration = sample_.duration;
            sample.idesc = sample_.idesc;
            sample.is_discontinuity = sample_.is_discontinuity;
            sample.is_sync = sample_.is_sync;
            sample.itrack = sample_.itrack;
            sample.offset = sample_.offset;
            sample.time = sample_.time - timestamp_offset_ms_;
            sample.ustime = sample.time*1000;
            sample.size = sample.data.size();
            return ec;
        }

        size_t FlvDemuxerBase::get_media_count(error_code & ec)
        {
            if (is_open(ec)) {
                return streams_info_.size();
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
                if (index >= streams_info_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_info_[index];
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
            boost::uint32_t total_size,
            boost::system::error_code & ec)
        {
            boost::uint32_t buffer_time = 0;
            buffer_time += (total_size / ((metadata_.datarate * 1000 * 2) / 8));
            std::cout << "size: " << total_size
                <<", buffer_time: " << buffer_time << std::endl;
            if (is_open(ec)) {
                return (sample_.time - timestamp_offset_ms_) + buffer_time*1000;
            }
            return 0;
        }

        boost::uint32_t FlvDemuxerBase::get_cur_time(
            boost::system::error_code & ec)
        {
            if (is_open(ec)) {
                return sample_.time - timestamp_offset_ms_;
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

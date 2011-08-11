// AsfDemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/asf/AsfGuid.h"
#include "ppbox/demux/asf/AsfObjectType.h"
#include "ppbox/demux/asf/AsfDemuxerBase.h"
using namespace ppbox::demux::error;

#include <framework/timer/TickCounter.h>
#include <framework/logger/LoggerStreamRecord.h>
using namespace framework::logger;

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("AsfDemuxerBase", 0)

namespace ppbox
{
    namespace demux
    {

        error_code AsfDemuxerBase::open(
            error_code & ec)
        {
            open_step_ = 0;
            is_open(ec);
            return ec;
        }

        bool AsfDemuxerBase::is_open(
            error_code & ec)
        {
            if (open_step_ == 2) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (size_t)-1) {
                ec = not_open;
                return false;
            }

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                assert(archive_);
                archive_ >> header_;
                if (!archive_) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                boost::uint32_t offset = archive_.tellg();
                if (!archive_.seekg(std::ios::off_type(header_.ObjectSize), std::ios_base::beg)) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                archive_.seekg(offset, std::ios_base::beg);
                assert(archive_);
                streams_.clear();
                stream_map_.clear();
                while (archive_ && offset < header_.ObjectSize) {
                    ASF_Object_Header obj_head;
                    archive_ >> obj_head;
                    if (obj_head.ObjectId == ASF_FILE_PROPERTIES_OBJECT) {
                        archive_ >> file_prop_;
                    } else if (obj_head.ObjectId == ASF_STREAM_PROPERTIES_OBJECT) {
                        ASF_Stream_Properties_Object_Data obj_data;
                        archive_ >> obj_data;
                        if ((size_t)obj_data.Flag.StreamNumber + 1 > streams_.size()) {
                            streams_.resize(obj_data.Flag.StreamNumber + 1);
                        }
                        streams_[obj_data.Flag.StreamNumber] = obj_data;
                        streams_[obj_data.Flag.StreamNumber].index_to_map_ = stream_map_.size();
                        //streams_[obj_data.Flag.StreamNumber].get_start_sample(start_samples_);
                        stream_map_.push_back(obj_data.Flag.StreamNumber);
                    }
                    offset += (boost::uint32_t)obj_head.ObjLength;
                    archive_.seekg(offset, std::ios_base::beg);
                }

                if (!archive_ || offset != header_.ObjectSize || stream_map_.empty()) {
                    open_step_ = (size_t)-1;
                    ec = bad_file_format;
                    return false;
                }

                open_step_ = 1;
            }

            if (open_step_ == 1) {
                archive_.seekg(std::ios::off_type(header_.ObjectSize), std::ios_base::beg);
                assert(archive_);
                ASF_Data_Object DataObject;
                archive_ >> DataObject;
                if (!archive_) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                object_parse_.packet.PayloadNum = 0;
                object_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                object_parse_.offset = archive_.tellg();
                next_object_offset_ = 0;
                object_payloads_.clear();
                is_discontinuity_ = true;

                if (file_prop_.MaximumDataPacketSize == file_prop_.MinimumDataPacketSize) {
                    fixed_packet_length_ = file_prop_.MaximumDataPacketSize;
                } else {
                    fixed_packet_length_ = 0;
                }
                buffer_parse_.packet.PayloadNum = 0;
                buffer_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                buffer_parse_.offset = object_parse_.offset;
                // 处理get_buffer_time有时没有初始化
                buffer_parse_.payload.StreamNum = stream_map_[0];
                buffer_parse_.payload.PresTime = streams_[buffer_parse_.payload.StreamNum].time_offset_ms_;

                ParseStatus status = object_parse_;
                while (!next_payload(status, ec)) {
                    if (status.payload.StreamNum >= streams_.size()) {
                        open_step_ = (size_t)-1;
                        ec = bad_file_format;
                        return false;
                    }
                    AsfStream & stream = streams_[status.payload.StreamNum];
                    if (!stream.ready_) {
                        stream.ready_ = true;
                        // 一些错误的文件，没有正确的time_offset，我们自己计算
                        stream.time_offset_ms_ = status.payload.PresTime;
                        stream.time_offset_us_ = (boost::uint64_t)status.payload.PresTime * 1000;
                        if (status.payload.StreamNum == stream_map_[0]) {
                            buffer_parse_.payload.MediaObjNum = status.payload.StreamNum;
                            buffer_parse_.payload.PresTime = status.payload.PresTime;
                        }
                        bool ready = true;
                        for (size_t i = 0; i < stream_map_.size(); ++i) {
                            if (!streams_[stream_map_[i]].ready_) {
                                ready = false;
                                break;
                            }
                        }
                        if (ready) {
                            for (size_t i = 0; i < stream_map_.size(); ++i) {
                                AsfStream & stream = streams_[stream_map_[i]];
                                LOG_S(Logger::kLevelDebug, "Stream: id = " 
                                    << stream.Flag.StreamNumber << ", time = " 
                                    << stream.time_offset_ms_ << "ms");
                            }
                            break;
                        }
                    }
                }

                if (!archive_) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }

                archive_.seekg(object_parse_.offset, std::ios_base::beg);
                assert(archive_);
                open_step_ = 2;
                if (fisrt_sample_dts_ == boost::uint64_t(-1)) {
                    fisrt_sample_dts_ = status.payload.PresTime;
                } else {
                    if (status.payload.PresTime < last_valid_sample_dts_
                         && ((last_valid_sample_dts_-status.payload.PresTime)/1000) < 120) {
                        skip_type_ = 1;
                    } else {
                        skip_type_ = 2;
                    }
                }
            }

            if (!ec) {
                open_step_ = 2;
                return true;
            } else {
                return false;
            }
        }

        error_code AsfDemuxerBase::close(
            error_code & ec)
        {
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        error_code AsfDemuxerBase::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            assert(skip_type_ < 3);
            if (0 == skip_type_ && !get_real_sample(sample, ec)) {
                last_valid_sample_dts_ = sample.dts;
                last_valid_sample_itrack_ = sample.itrack;
            } else if (1 == skip_type_ && !get_end_time_sample(sample, ec)) { // EndTime
            } else if (2 == skip_type_ && !get_key_sample(sample, ec)) { // KeyFrame
            }
            return ec;
        }

        error_code AsfDemuxerBase::get_real_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            archive_.seekg(object_parse_.offset, std::ios_base::beg);
            assert(archive_);
            /*
            if (!start_samples_.empty()) {
                sample = start_samples_.back();
                start_samples_.pop_back();
                return ec = error_code();
            }
            */
            while (true) {
                next_payload(object_parse_, ec);
                //if (fixed_packet_length_ && ec == bad_file_format) {
                //    archive_.clear();
                //    archive_.seekg(object_parse_.offset_packet + fixed_packet_length_, std::ios_base::beg);
                //    if (archive_) {
                //        object_parse_.offset = object_parse_.offset_packet + fixed_packet_length_;
                //        object_parse_.packet.PayloadNum = 0;
                //        object_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                //        continue;
                //    } else {
                //        archive_.clear();
                //        ec = file_stream_error;
                //    }
                //}
                if (ec) {
                    return ec;
                }
                // Object not continue
                if (false  == object_payloads_.empty() 
                    && (object_payloads_[0].StreamNum != object_parse_.payload.StreamNum 
                    || object_payloads_[0].MediaObjNum != object_parse_.payload.MediaObjNum)) {
                        object_payloads_.clear();
                        next_object_offset_ = 0;
                        is_discontinuity_ = true;
                }
                if (next_object_offset_ != object_parse_.payload.OffsetIntoMediaObj) {
                    // Payload not continue
                    object_payloads_.clear();
                    next_object_offset_ = 0;
                    is_discontinuity_ = true;
                }
                object_payloads_.push_back(object_parse_.payload);
                next_object_offset_ += object_parse_.payload.PayloadLength;
                if (next_object_offset_ == object_parse_.payload.MediaObjectSize) {
                    AsfStream & stream = streams_[object_parse_.payload.StreamNum];
                    stream.next_id_ = object_parse_.payload.MediaObjNum;
                    sample.data.resize(object_parse_.payload.MediaObjectSize);
                    boost::uint8_t * data_buf = &sample.data[0];
                    for (size_t i = 0; i < object_payloads_.size(); ++i) {
                        archive_.seekg(object_payloads_[i].data_offset, std::ios_base::beg);
                        archive_ >> framework::container::make_array(data_buf, object_payloads_[i].PayloadLength);
                        data_buf += object_payloads_[i].PayloadLength;
                    }
                    sample.itrack = stream.index_to_map_;
                    //sample.time = object_parse_.payload.PresTime - stream.time_offset_ms;
                    //sample.ustime = (boost::uint64_t)object_parse_.payload.PresTime * 1000 - stream.time_offset_us;
                    sample.time = object_parse_.payload.PresTime - fisrt_sample_dts_;
                    sample.ustime = sample.time*1000;
                    sample.offset = 0;
                    sample.size = object_parse_.payload.MediaObjectSize;
                    sample.duration = 0;
                    sample.idesc = 0;
                    sample.dts = object_parse_.payload.PresTime;
                    sample.cts_delta = (boost::uint32_t)-1;
                    sample.is_sync = object_parse_.payload.KeyFrameBit;
                    sample.is_discontinuity = is_discontinuity_;
                    object_payloads_.clear();
                    is_discontinuity_ = false;
                    next_object_offset_ = 0;
                    break;
                }
            }
            return ec = error_code();
        }

        error_code AsfDemuxerBase::get_sample_without_data(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            archive_.seekg(object_parse_.offset, std::ios_base::beg);
            assert(archive_);
            while (true) {
                next_payload(object_parse_, ec);
                if (ec) {
                    return ec;
                }
                // Object not continue
                if (false  == object_payloads_.empty() 
                    && (object_payloads_[0].StreamNum != object_parse_.payload.StreamNum 
                    || object_payloads_[0].MediaObjNum != object_parse_.payload.MediaObjNum)) {
                        object_payloads_.clear();
                        next_object_offset_ = 0;
                        is_discontinuity_ = true;
                }
                if (next_object_offset_ != object_parse_.payload.OffsetIntoMediaObj) {
                    // Payload not continue
                    object_payloads_.clear();
                    next_object_offset_ = 0;
                    is_discontinuity_ = true;
                }
                object_payloads_.push_back(object_parse_.payload);
                next_object_offset_ += object_parse_.payload.PayloadLength;
                if (next_object_offset_ == object_parse_.payload.MediaObjectSize) {
                    AsfStream & stream = streams_[object_parse_.payload.StreamNum];
                    stream.next_id_ = object_parse_.payload.MediaObjNum;
                    sample.itrack = stream.index_to_map_;
                    sample.time = object_parse_.payload.PresTime - stream.time_offset_ms_;
                    sample.ustime = (boost::uint64_t)object_parse_.payload.PresTime * 1000 - stream.time_offset_us_;
                    sample.offset = 0;
                    sample.size = object_parse_.payload.MediaObjectSize;
                    sample.duration = 0;
                    sample.idesc = 0;
                    sample.dts = object_parse_.payload.PresTime;
                    sample.cts_delta = 0;
                    sample.is_sync = object_parse_.payload.KeyFrameBit;
                    sample.is_discontinuity = is_discontinuity_;
                    object_payloads_.clear();
                    is_discontinuity_ = false;
                    next_object_offset_ = 0;
                    break;
                }
            }
            return ec = error_code();
        }

        size_t AsfDemuxerBase::get_media_count(
            error_code & ec)
        {
            if (is_open(ec))
                return stream_map_.size();
            return 0;
        }

        error_code AsfDemuxerBase::get_media_info(
            size_t index, 
            MediaInfo & info, 
            error_code & ec)
        {
            if (is_open(ec)) {
                if (index >= stream_map_.size()) {
                    ec = framework::system::logic_error::out_of_range;
                } else {
                    info = streams_[stream_map_[index]];
                    // 添加直播的配置信息
                    if (MEDIA_TYPE_VIDE == info.type) {
                        info.format_data = streams_[stream_map_[index]].Video_Media_Type.FormatData.CodecSpecificData;
                    }
                }
            }
            return ec;
        }

        boost::uint32_t AsfDemuxerBase::get_duration(
            error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }

        boost::uint32_t AsfDemuxerBase::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            if (fixed_packet_length_ == 0) {
                ec = error::not_support;
                return 0;
            }
            boost::uint32_t beg = archive_.tellg();
            archive_.seekg(0, std::ios_base::end);
            boost::uint32_t end = archive_.tellg();
            assert(archive_);
            boost::uint32_t n = (end - buffer_parse_.offset) / fixed_packet_length_;
            boost::uint32_t off = buffer_parse_.offset + (n - 1) * fixed_packet_length_;
            if (off > buffer_parse_.offset && off >= beg) {
                buffer_parse_.offset = off;
                buffer_parse_.packet.PayloadNum = 0; // start from packet
                buffer_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                archive_.seekg(buffer_parse_.offset, std::ios_base::beg);
                next_payload(buffer_parse_, ec);
                buffer_parse_.offset = off; // recover
            }
            if (buffer_parse_.payload.StreamNum < streams_.size()) {
                AsfStream & stream = streams_[buffer_parse_.payload.StreamNum];
                return buffer_parse_.payload.PresTime - stream.time_offset_ms_;
            } else {
                ec = bad_file_format;
                return 0;
            }
        }

        boost::uint32_t AsfDemuxerBase::get_cur_time(
            error_code & ec)
        {
            if (is_open(ec) && object_parse_.payload.StreamNum < streams_.size()) {
                AsfStream & stream = streams_[object_parse_.payload.StreamNum];
                return object_parse_.payload.PresTime - stream.time_offset_ms_;
            }
            return 0;
        }

        boost::uint64_t AsfDemuxerBase::seek_to(
            boost::uint32_t & time, 
            error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }

        error_code AsfDemuxerBase::next_packet(
            ParseStatus & parse_status,  
            error_code & ec)
        {
            if (archive_ >> parse_status.packet) {
                ++parse_status.num_packet;
                return ec = error_code();
            } else if (archive_.failed()) {
                archive_.clear();
                return ec = bad_file_format;
            } else {
                archive_.clear();
                return ec = file_stream_error;
            }
        }

        bool AsfDemuxerBase::is_video_sample(Sample & sample)
        {
            bool res = false;
            boost::system::error_code ec;
            boost::uint32_t stream_count = get_media_count(ec);
            MediaInfo media_info;
            if (!ec) {
                for (int index = 0; index < stream_count; ++index) {
                    get_media_info(index, media_info, ec);
                    if (!ec) {
                        if (media_info.type == MEDIA_TYPE_VIDE && sample.itrack == index) {
                            res = true;
                            break;
                        }
                    }
                }
            }
            return res;
        }

        error_code AsfDemuxerBase::get_key_sample(
            Sample & sample,
            error_code & ec)
        {
            while (true) {
                get_real_sample(sample, ec);
                if (!ec) {
                    if (sample.is_sync && is_video_sample(sample)) {
                        fisrt_sample_dts_ = sample.dts;
                        skip_type_ = 0;
                        break;
                    }
                } else {
                    break;
                }
            }
            return ec;
        }

        error_code AsfDemuxerBase::get_end_time_sample(
            Sample & sample,
            error_code & ec)
        {
            while (true) {
                get_sample_without_data(sample, ec);
                if (!ec) {
                    if (sample.dts >= last_valid_sample_dts_
                        && sample.itrack == last_valid_sample_itrack_) {
                            get_real_sample(sample, ec);
                            skip_type_ = 0;
                            break;
                    }
                }else {
                    break;
                }
            }
            return ec;
        }

        error_code AsfDemuxerBase::next_payload(
            ParseStatus & parse_status,  
            error_code & ec)
        {
            if (parse_status.packet.PayloadNum == 0) {
                if (parse_status.packet.PayLoadParseInfo.PaddingLength) {
                    archive_.seekg(parse_status.packet.PayLoadParseInfo.PaddingLength, std::ios_base::cur);
                    if (archive_) {
                        parse_status.offset += parse_status.packet.PayLoadParseInfo.PaddingLength;
                        parse_status.packet.PayLoadParseInfo.PaddingLength = 0;
                    } else {
                        archive_.clear();
                        return ec = file_stream_error;
                    }
                }
                parse_status.offset_packet = parse_status.offset;
                if (next_packet(parse_status, ec)) {
                    parse_status.packet.PayloadNum = 0;
                    parse_status.packet.PayLoadParseInfo.PaddingLength = 0;
                    return ec;
                } else {
                    parse_status.offset = archive_.tellg();
                }
            }
            parse_status.payload.set_packet(parse_status.packet);
            if (archive_ >> parse_status.payload) {
                parse_status.offset = parse_status.payload.data_offset + parse_status.payload.PayloadLength;
                --parse_status.packet.PayloadNum;
                ++parse_status.num_payload;
                return ec = error_code();
            } else if (archive_.failed()) {
                archive_.clear();
                return ec = bad_file_format;
            } else {
                archive_.clear();
                return ec = file_stream_error;
            }
        }

    }
}

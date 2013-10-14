// AsfDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/asf/AsfDemuxer.h"
using namespace ppbox::demux::error;

#include <ppbox/avformat/asf/AsfGuid.h>
using namespace ppbox::avformat;
using namespace ppbox::avformat::error;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/LogicError.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.AsfDemuxer", framework::logger::Warn)

namespace ppbox
{
    namespace demux
    {

        AsfDemuxer::AsfDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_(size_t(-1))
            , timestamp_offset_ms_(boost::uint64_t(-1))
        {
        }

        AsfDemuxer::~AsfDemuxer()
        {
        }

        error_code AsfDemuxer::open(
            error_code & ec)
        {
            open_step_ = 0;
            is_open(ec);
            return ec;
        }

        bool AsfDemuxer::is_open(
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

            assert(archive_);

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                assert(archive_);
                archive_ >> header_;
                if (!archive_) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                boost::uint64_t offset = archive_.tellg();
                if (!archive_.seekg(std::ios::off_type(header_.ObjLength), std::ios_base::beg)) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                archive_.seekg(offset, std::ios_base::beg);
                assert(archive_);
                streams_.clear();
                stream_map_.clear();
                while (archive_ && offset < header_.ObjLength) {
                    AsfObjectHeader obj_head;
                    archive_ >> obj_head;
                    if (obj_head.ObjectId == ASF_FILE_PROPERTIES_OBJECT) {
                        archive_ >> file_prop_;
                        object_parse_.context.max_packet_size = file_prop_.MaximumDataPacketSize;
                        buffer_parse_.context.max_packet_size = file_prop_.MaximumDataPacketSize;
                    } else if (obj_head.ObjectId == ASF_STREAM_PROPERTIES_OBJECT) {
                        AsfStreamPropertiesObjectData obj_data;
                        archive_ >> obj_data;
                        size_t index = streams_.size();
                        if ((size_t)obj_data.Flag.StreamNumber + 1 > stream_map_.size()) {
                            stream_map_.resize(obj_data.Flag.StreamNumber + 1, size_t(-1));
                            stream_map_[obj_data.Flag.StreamNumber] = index;
                        }
                        streams_.push_back(AsfStream(obj_data));
                        streams_.back().index = index;
                    } else {
                        archive_.seekg(obj_head.ObjLength - 24, std::ios::cur);
                    }
                    offset += (boost::uint64_t)obj_head.ObjLength;
                    archive_.seekg(offset, std::ios_base::beg);
                } // while

                if (!archive_ || offset != header_.ObjLength || stream_map_.empty()) {
                    open_step_ = (size_t)-1;
                    ec = bad_media_format;
                    return false;
                }

                parses_.resize(streams_.size());
                open_step_ = 1;
            }

            if (open_step_ == 1) {
                archive_.seekg(std::ios::off_type(header_.ObjLength), std::ios_base::beg);
                assert(archive_);
                AsfDataObject DataObject;
                archive_ >> DataObject;
                if (!archive_) {
                    archive_.clear();
                    ec = file_stream_error;
                    return false;
                }
                header_offset_ = archive_.tellg();
                object_parse_.packet.PayloadNum = 0;
                object_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                object_parse_.offset = header_offset_;

                if (file_prop_.MaximumDataPacketSize == file_prop_.MinimumDataPacketSize) {
                    fixed_packet_length_ = file_prop_.MaximumDataPacketSize;
                } else {
                    fixed_packet_length_ = 0;
                }
                buffer_parse_.packet.PayloadNum = 0;
                buffer_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                buffer_parse_.offset = header_offset_;

                if (!next_payload(archive_, object_parse_, ec)) {
                    if (object_parse_.payload.StreamNum >= stream_map_.size()) {
                        open_step_ = (size_t)-1;
                        ec = bad_media_format;
                        return false;
                    }
                    timestamp_offset_ms_ = object_parse_.payload.PresTime;
                    buffer_parse_.payload.PresTime = object_parse_.payload.PresTime;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        streams_[i].start_time = timestamp_offset_ms_;
                    }
                    object_parse_.packet.PayloadNum = 0;
                    object_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                    object_parse_.offset = header_offset_;
                    archive_.seekg(object_parse_.offset, std::ios_base::beg);
                    assert(archive_);
                    open_step_ = 2;
                    on_open();
                }
            }

            if (!ec) {
                assert(open_step_ == 2);
                return true;
            } else {
                return false;
            }
        }

        bool AsfDemuxer::is_open(
            error_code & ec) const
        {
            if (open_step_ == 2) {
                ec = error_code();
                return true;
            } else {
                ec = not_open;
                return false;
            }
        }

        error_code AsfDemuxer::close(
            error_code & ec)
        {
            if (open_step_ == 2) {
                on_close();
            }
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t AsfDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (is_open(ec)) {
                dts.assign(dts.size(), timestamp_offset_ms_);
                object_parse_.packet.PayloadNum = 0;
                object_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                object_parse_.offset = header_offset_;
                for (size_t i = 0; i < parses_.size(); ++i) {
                    parses_[i].clear();
                }
                buffer_parse_.packet.PayloadNum = 0;
                buffer_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                buffer_parse_.offset = header_offset_;
                ec.clear();
                return header_offset_;
            } else {
                return 0;
            }
        }

        boost::uint64_t AsfDemuxer::get_duration(
            error_code & ec) const
        {
            ec = framework::system::logic_error::not_supported;
            return ppbox::data::invalid_size;
        }

        size_t AsfDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec))
                return streams_.size();
            return 0;
        }

        error_code AsfDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            error_code & ec) const
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

        error_code AsfDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            // 因为一个帧可能存在于多个payload，所有如果get_sample失败，必须回退文件指针到帧的第一个payload上，但是解析不会重复
            boost::uint64_t offset = archive_.tellg();
            archive_.seekg(object_parse_.offset, std::ios_base::beg);
            //assert(archive_);
            /*
            if (!start_samples_.empty()) {
                sample = start_samples_.back();
                start_samples_.pop_back();
                return ec = error_code();
            }
            */
            while (true) {
                next_payload(archive_, object_parse_, ec);
                if (ec) {
                    archive_.seekg(offset, std::ios_base::beg);
                    return ec;
                }
                if (object_parse_.payload.StreamNum >= stream_map_.size()) {
                    ec = bad_media_format;
                    return ec;
                }
                size_t index = stream_map_[object_parse_.payload.StreamNum];
                if (index >= streams_.size()) {
                    ec = bad_media_format;
                    return ec;
                }
                AsfParse & parse(parses_[index]);
                if (parse.add_payload(object_parse_.context, object_parse_.payload)) {
                    AsfStream & stream = streams_[index];
                    BasicDemuxer::begin_sample(sample);
                    sample.itrack = index;
                    sample.flags = 0;
                    if (parse.is_sync_frame())
                        sample.flags |= Sample::f_sync;
                    if (parse.is_discontinuity())
                        sample.flags |= Sample::f_discontinuity;
                    sample.dts = parse.dts();
                    sample.cts_delta = boost::uint32_t(-1);
                    sample.duration = 0;
                    sample.size = object_parse_.payload.MediaObjectSize;
                    sample.stream_info = &stream;
                    parse.clear(BasicDemuxer::datas());
                    BasicDemuxer::end_sample(sample);
                    break;
                }
            }
            return ec = error_code();
        }

        boost::uint32_t AsfDemuxer::probe(
            boost::uint8_t const * hbytes, 
            size_t hsize) const
        {
            using framework::string::UUID;
            using framework::string::Uuid;
            if (hsize < sizeof(UUID)) {
                return 0;
            }
            Uuid uuid;
            Uuid::bytes_type bytes;
            memcpy(bytes.elems, hbytes, sizeof(UUID));
            uuid.from_little_endian_bytes(bytes);
            if (uuid == ASF_HEADER_OBJECT) {
                return SCOPE_MAX;
            }
            return 0;
        }

        boost::uint64_t AsfDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (is_open(ec)) {
                return timestamp().time();
            }
            return 0;
        }

        boost::uint64_t AsfDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            if (fixed_packet_length_ == 0) {
                ec = framework::system::logic_error::not_supported;
                return 0;
            }
            boost::uint64_t beg = archive_.tellg();
            archive_.seekg(0, std::ios_base::end);
            boost::uint64_t end = archive_.tellg();
            assert(archive_);
            if (end >= buffer_parse_.offset + fixed_packet_length_ * 2) {
                boost::uint64_t n = (end - buffer_parse_.offset) / fixed_packet_length_;
                boost::uint64_t off = buffer_parse_.offset + (n - 1) * fixed_packet_length_;
                buffer_parse_.offset = off;
                buffer_parse_.packet.PayloadNum = 0; // start from packet
                buffer_parse_.packet.PayLoadParseInfo.PaddingLength = 0;
                archive_.seekg(buffer_parse_.offset, std::ios_base::beg);
                next_payload(archive_, buffer_parse_, ec);
                buffer_parse_.offset = off; // recover
            }
            archive_.seekg(beg, std::ios_base::beg);
            return timestamp().const_adjust(0, buffer_parse_.payload.PresTime);
        }

        error_code AsfDemuxer::next_packet(
            AsfIArchive & archive, 
            ParseStatus & parse_status, 
            error_code & ec) const
        {
            if (archive >> parse_status.packet) {
                ++parse_status.num_packet;
                return ec = error_code();
            } else if (archive.failed()) {
                archive.clear();
                return ec = bad_media_format;
            } else {
                archive.clear();
                return ec = file_stream_error;
            }
        }

        error_code AsfDemuxer::next_payload(
            AsfIArchive & archive, 
            ParseStatus & parse_status,  
            error_code & ec) const
        {
            archive.context(&parse_status.context);
            if (parse_status.packet.PayloadNum == 0) {
                if (parse_status.packet.PayLoadParseInfo.PaddingLength) {
                    archive.seekg(parse_status.packet.PayLoadParseInfo.PaddingLength, std::ios_base::cur);
                    if (archive) {
                        parse_status.offset += parse_status.packet.PayLoadParseInfo.PaddingLength;
                        parse_status.packet.PayLoadParseInfo.PaddingLength = 0;
                    } else {
                        archive.clear();
                        return ec = file_stream_error;
                    }
                }
                parse_status.offset_packet = parse_status.offset;
                if (next_packet(archive, parse_status, ec)) {
                    parse_status.packet.PayloadNum = 0;
                    parse_status.packet.PayLoadParseInfo.PaddingLength = 0;
                    return ec;
                } else {
                    parse_status.offset = archive.tellg();
                }
            }
            if (archive >> parse_status.payload) {
                archive.seekg(parse_status.payload.PayloadLength, std::ios_base::cur);
                if (archive) {
                    parse_status.offset = parse_status.context.payload_data_offset + parse_status.payload.PayloadLength;
                    --parse_status.packet.PayloadNum;
                    ++parse_status.num_payload;
                    return ec = error_code();
                } else {
                    archive.clear();
                    return ec = file_stream_error;
                }
            } else if (archive.failed()) {
                archive.clear();
                return ec = bad_media_format;
            } else {
                archive.clear();
                return ec = file_stream_error;
            }
        }

    } // namespace demux
} // namespace ppbox

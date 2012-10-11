// FlvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/flv/FlvDemuxer.h"
using namespace ppbox::demux::error;
using namespace ppbox::avformat;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/BytesOrder.h>
#include <framework/timer/TimeCounter.h>
using namespace framework::logger;
using namespace framework::system;
using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.FlvDemuxer", Debug)

#include <iostream>

namespace ppbox
{
    namespace demux
    {

        FlvDemuxer::FlvDemuxer(
            std::basic_streambuf<boost::uint8_t> & buf)
            : DemuxerBase(buf)
            , archive_(buf)
            , open_step_((boost::uint64_t)-1)
            , header_offset_(0)
            , parse_offset_(0)
            , timestamp_offset_ms_(0)
            , current_time_(0)
        {
            streams_.resize(2);
        }

        FlvDemuxer::~FlvDemuxer()
        {
        }

        error_code FlvDemuxer::open(
            error_code & ec)
        {
            open_step_ = 0;
            parse_offset_ = 0;
            ec.clear();
            is_open(ec);
            return ec;
        }

        bool FlvDemuxer::is_open(
            error_code & ec)
        {
            if (open_step_ == 3) {
                ec = error_code();
                return true;
            }

            if (open_step_ == (boost::uint64_t)-1) {
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
                    streams_.resize((size_t)FlvTagType::META + 1);
                    if (flv_header_.TypeFlagsAudio) {
                        streams_[(size_t)FlvTagType::AUDIO].index = stream_map_.size();
                        stream_map_.push_back((size_t)FlvTagType::AUDIO);
                    }
                    if (flv_header_.TypeFlagsVideo) {
                        streams_[(size_t)FlvTagType::VIDEO].index = stream_map_.size();
                        stream_map_.push_back((size_t)FlvTagType::VIDEO);
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
                    if (flv_tag_.Type == FlvTagType::META) {
                        parse_metadata(flv_tag_);
                    }
                    std::vector<boost::uint8_t> codec_data;
                    archive_.seekg(std::streamoff(flv_tag_.data_offset), std::ios_base::beg);
                    util::serialization::serialize_collection(archive_, codec_data, (boost::uint64_t)flv_tag_.DataSize);
                    archive_.seekg(4, std::ios_base::cur);
                    assert(archive_);
                    parse_offset_ = (boost::uint64_t)archive_.tellg();
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
            } else {
                if (3 == open_step_) {
                    boost::uint64_t duration = 0;
                    boost::uint64_t filesize = 0;
                    if (metadata_.duration != 0) {
                        duration = metadata_.duration * 1000; // ms
                    }
                    if (metadata_.filesize != 0) {
                        filesize = metadata_.filesize;
                    }
                }
            }

            return true;
        }

        error_code FlvDemuxer::close(error_code & ec)
        {
            header_offset_ = 0;
            timestamp_offset_ms_ = 0;
            parse_offset_ = 0;
            ec.clear();
            open_step_ = boost::uint64_t(-1);
            return ec;
        }

        error_code FlvDemuxer::reset(
            error_code & ec)
        {
            ec.clear();
            parse_offset_ = header_offset_;
            return ec;
        }

        boost::uint64_t FlvDemuxer::seek(
            boost::uint64_t & time, 
            error_code & ec)
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

        boost::uint64_t FlvDemuxer::get_duration(
            error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }

        size_t FlvDemuxer::get_stream_count(error_code & ec)
        {
            if (is_open(ec)) {
                return stream_map_.size();
            } else {
                return 0;
            }
        }

        error_code FlvDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            error_code & ec)
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

        error_code FlvDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            archive_.seekg(parse_offset_, std::ios_base::beg);
            //assert(archive_);
            framework::timer::TimeCounter tc;
            if (get_tag(flv_tag_, ec)) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                return ec;
            }
            if (flv_tag_.Type == FlvTagType::VIDEO) {
                current_time_ = flv_tag_.Timestamp;
            }
            if (tc.elapse() > 10) {
                LOG_DEBUG("[get_tag], elapse " << tc.elapse());
            }
            parse_offset_ = (boost::uint64_t)archive_.tellg();
            if (flv_tag_.is_sample) {
                assert(flv_tag_.Type < streams_.size() && 
                    streams_[(size_t)flv_tag_.Type].index < stream_map_.size());
                FlvStream const & stream = streams_[(size_t)flv_tag_.Type];
                sample.itrack = stream.index;
                sample.idesc = 0;
                sample.flags = 0;
                if (flv_tag_.is_sync)
                    sample.flags |= Sample::sync;
                boost::uint64_t timestamp = timestamp_.transfer((boost::uint64_t)flv_tag_.Timestamp);
                sample.time = (boost::uint64_t)timestamp - timestamp_offset_ms_;
                sample.ustime = (timestamp - timestamp_offset_ms_) * 1000;
                sample.dts = timestamp - timestamp_offset_ms_;
                sample.cts_delta = flv_tag_.cts_delta;
                sample.us_delta = 1000*sample.cts_delta;
                sample.duration = 0;
                sample.size = flv_tag_.DataSize;
                sample.blocks.clear();
                sample.blocks.push_back(FileBlock(flv_tag_.data_offset, flv_tag_.DataSize));
            } else if (flv_tag_.Type == FlvTagType::META) {
                LOG_DEBUG("[get_sample] script data: " << flv_tag_.DataTag.Name.String.StringData);
                return get_sample(sample, ec);
            } else if (flv_tag_.Type == FlvTagType::AUDIO) {
                if (flv_tag_.AudioHeader.SoundFormat == FlvSoundCodec::AAC
                    && flv_tag_.AudioHeader.AACPacketType == 0) {
                        LOG_DEBUG("[get_sample] duplicate aac sequence header");
                        return get_sample(sample, ec);
                }
                ec = bad_file_format;
            } else if (flv_tag_.Type == FlvTagType::VIDEO) {
                if (flv_tag_.VideoHeader.CodecID == FlvVideoCodec::H264) {
                    if (flv_tag_.VideoHeader.AVCPacketType == 0) {
                        LOG_DEBUG("[get_sample] duplicate aac sequence header");
                        return get_sample(sample, ec);
                    } else if (flv_tag_.VideoHeader.AVCPacketType == 2) {
                        LOG_DEBUG("[get_sample] end of avc sequence");
                        return get_sample(sample, ec);
                    }
                } 
                ec = bad_file_format;
            } else {
                ec = bad_file_format;
            }
            return ec;
        }

        boost::uint64_t FlvDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t beg = archive_.tellg();
            archive_.seekg(0, std::ios_base::end);
            boost::uint64_t end = archive_.tellg();
            archive_.seekg(beg, std::ios_base::beg);
            assert(archive_);
            boost::uint64_t time = 0;
            if (flv_tag_.Timestamp - timestamp_offset_ms_ > 1000) {
                time = (flv_tag_.Timestamp - timestamp_offset_ms_) * end / parse_offset_;
            } else if (metadata_.datarate) {
                time = end / metadata_.datarate;
            } else {
                time = 0;
            }
            return time;
        }

        boost::uint64_t FlvDemuxer::get_cur_time(
            error_code & ec)
        {
            if (is_open(ec)) {
                // return flv_tag_.Timestamp - timestamp_offset_ms_;
                 return current_time_ - timestamp_offset_ms_;
            }
            return 0;
        }

        boost::uint64_t FlvDemuxer::get_offset(
            boost::uint64_t & time, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            ec = error::not_support;
            return 0;
        }

        void FlvDemuxer::set_stream(std::basic_streambuf<boost::uint8_t> & buf)
        {
            archive_.rdbuf(&buf);
        }

        void FlvDemuxer::set_time_offset(boost::uint64_t offset)
        {
            timestamp_offset_ms_ = boost::uint64_t(offset / 1000);
        }

        boost::uint64_t FlvDemuxer::get_time_offset() const
        {
            return (boost::uint64_t)timestamp_offset_ms_ * 1000;
        }

        error_code FlvDemuxer::get_tag(
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

        void FlvDemuxer::parse_metadata(
            FlvTag const & metadata_tag)
        {
            std::vector<FlvDataObjectProperty> const & variables = 
                (metadata_tag.DataTag.Value.Type == AMFDataType::MIXEDARRAY)
                ? metadata_tag.DataTag.Value.ECMAArray.Variables
                : metadata_tag.DataTag.Value.Object.ObjectProperties;
            for (size_t i = 0; i < variables.size(); ++i) {
                FlvDataObjectProperty const & property = variables[i];
                if (property.PropertyName.StringData == "datarate") {
                    metadata_.datarate = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "width") {
                    metadata_.width = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "height") {
                    metadata_.height = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "framerate") {
                    metadata_.framerate = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "audiosamplerate") {
                    metadata_.audiosamplerate = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "audiosamplesize") {
                    metadata_.audiosamplesize = (boost::uint32_t)property.PropertyData.Double;
                }
                if (property.PropertyName.StringData == "audiosamplesize") {
                    metadata_.audiosamplesize = (boost::uint32_t)property.PropertyData.Double;
                }
            }
        }

    }
}

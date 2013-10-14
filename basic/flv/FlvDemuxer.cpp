// FlvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/flv/FlvDemuxer.h"
using namespace ppbox::demux::error;

using namespace ppbox::avformat;
using namespace ppbox::avformat::error;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
#include <framework/system/LogicError.h>
#include <framework/timer/TimeCounter.h>
using namespace framework::system;

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.FlvDemuxer", framework::logger::Warn)

#include "ppbox/demux/basic/flv/FlvStream.h"

namespace ppbox
{
    namespace demux
    {

        FlvDemuxer::FlvDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_((size_t)-1)
            , header_offset_(0)
            , parse_offset_(0)
            , parse_offset2_(0)
            , timestamp_offset_ms_(0)
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
            parse_offset_ = parse_offset2_ = 0;
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

            if (open_step_ == (size_t)-1) {
                ec = error::not_open;
                return false;
            }

            assert(archive_);

            if (open_step_ == 0) {
                archive_.seekg(0, std::ios_base::beg);
                assert(archive_);
                archive_ >> flv_header_;
                if (archive_) {
                    stream_map_.resize((size_t)FlvTagType::DATA + 1, size_t(-1));
                    size_t n = 0;
                    if (flv_header_.TypeFlagsAudio) {
                        stream_map_[(size_t)FlvTagType::AUDIO] = n;
                        ++n;
                    }
                    if (flv_header_.TypeFlagsVideo) {
                        stream_map_[(size_t)FlvTagType::VIDEO] = n;
                        ++n;
                    }
                    streams_.resize(n);
                    open_step_ = 1;
                    parse_offset_ = std::ios::off_type(flv_header_.DataOffset) + 4; // + 4 PreTagSize
                } else {
                    if (archive_.failed()) {
                        ec = bad_media_format;
                    } else {
                        ec = file_stream_error;
                    }
                    archive_.clear();
                }
            }

            if (open_step_ == 1) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                while (!get_tag(flv_tag_, ec)) {
                    if (flv_tag_.Type == FlvTagType::DATA 
                        && flv_tag_.DataTag.Name == "onMetaData") {
                        metadata_.from_data(flv_tag_.DataTag.Value);
                    }
                    if (flv_tag_.Type >= stream_map_.size() ||
                        stream_map_[(size_t)flv_tag_.Type] >= streams_.size()) {
                            continue;
                    }
                    std::vector<boost::uint8_t> codec_data;
                    archive_.seekg(std::streamoff(flv_tag_.data_offset), std::ios_base::beg);
                    assert(archive_);
                    util::serialization::serialize_collection(archive_, codec_data, (boost::uint64_t)flv_tag_.DataSize);
                    archive_.seekg(4, std::ios_base::cur);
                    assert(archive_);
                    parse_offset_ = (boost::uint64_t)archive_.tellg();
                    size_t index = stream_map_[(size_t)flv_tag_.Type];
                    streams_[index] = FlvStream(flv_tag_, codec_data, metadata_);
                    streams_[index].index = index;
                    streams_[index].ready = true;
                    bool ready = true;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        if (!streams_[i].ready) {
                            ready = false;
                            break;
                        }
                    }
                    if (ready)
                        break;
                }
                if (!ec) {
                    open_step_ = 2;
                }
            }

            if (open_step_ == 2) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                while (!get_tag(flv_tag_, ec)) {
                    if (flv_tag_.Type < stream_map_.size() &&
                        stream_map_[(size_t)flv_tag_.Type] < stream_map_.size()) {
                            break;
                    }
                }
                if (!ec) {
                    archive_.seekg(parse_offset_, std::ios_base::beg);
                    assert(archive_);
                    timestamp_offset_ms_ = flv_tag_.Timestamp;
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        streams_[i].start_time = timestamp_offset_ms_;
                    }
                    header_offset_ = parse_offset_;
                    open_step_ = 3;
                }
            }

            if (ec) {
                archive_.clear();
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                return false;
            } else {
                assert(open_step_ == 3);
                on_open();
                return true;
            }
        }

        bool FlvDemuxer::is_open(
            error_code & ec) const
        {
            if (open_step_ == 3) {
                ec = error_code();
                return true;
            } else {
                ec = error::not_open;
                return false;
            }
        }

        error_code FlvDemuxer::close(error_code & ec)
        {
            if (open_step_ == 3) {
                on_close();
            }
            streams_.clear();
            stream_map_.clear();
            header_offset_ = 0;
            timestamp_offset_ms_ = 0;
            parse_offset_ = 0;
            ec.clear();
            open_step_ = size_t(-1);
            return ec;
        }

        boost::uint64_t FlvDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (is_open(ec)) {
                //if (time == 0) {
                    ec.clear();
                    dts.assign(dts.size(), timestamp_offset_ms_); // TO BE FIXED
                    parse_offset_ = header_offset_;
                    return header_offset_;
                //} else {
                //    ec = framework::system::logic_error::not_supported;
                //    return 0;
                //}
            } else {
                return 0;
            }
        }

        boost::uint64_t FlvDemuxer::get_duration(
            error_code & ec) const
        {
            ec = framework::system::logic_error::not_supported;
            return ppbox::data::invalid_size;
        }

        size_t FlvDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec)) {
                return streams_.size();
            } else {
                return 0;
            }
        }

        error_code FlvDemuxer::get_stream_info(
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

        error_code FlvDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            archive_.seekg(parse_offset_, std::ios_base::beg);
            assert(archive_);
            framework::timer::TimeCounter tc;
            if (get_tag(flv_tag_, ec)) {
                archive_.seekg(parse_offset_, std::ios_base::beg);
                assert(archive_);
                return ec;
            }
            if (tc.elapse() > 10) {
                LOG_DEBUG("[get_tag], elapse " << tc.elapse());
            }
            parse_offset_ = (boost::uint64_t)archive_.tellg();
            if (flv_tag_.is_sample) {
                assert(flv_tag_.Type < stream_map_.size());
                size_t index = stream_map_[(size_t)flv_tag_.Type];
                assert(index < streams_.size());
                FlvStream const & stream = streams_[index];
                BasicDemuxer::begin_sample(sample);
                sample.itrack = index;
                sample.flags = 0;
                if (flv_tag_.is_sync)
                    sample.flags |= Sample::f_sync;
                sample.dts = timestamp_.transfer((boost::uint64_t)flv_tag_.Timestamp);;
                sample.cts_delta = flv_tag_.cts_delta;
                sample.duration = 0;
                sample.size = flv_tag_.DataSize;
                sample.stream_info = &stream;
                BasicDemuxer::push_data(flv_tag_.data_offset, flv_tag_.DataSize);
                BasicDemuxer::end_sample(sample);
            } else if (flv_tag_.Type == FlvTagType::DATA) {
                LOG_DEBUG("[get_sample] script data: " << flv_tag_.DataTag.Name.as<FlvDataString>().StringData);
                return get_sample(sample, ec);
            } else if (flv_tag_.Type == FlvTagType::AUDIO) {
                if (flv_tag_.AudioHeader.SoundFormat == FlvSoundCodec::AAC
                    && flv_tag_.AudioHeader.AACPacketType == 0) {
                        LOG_DEBUG("[get_sample] duplicate aac sequence header");
                        return get_sample(sample, ec);
                }
                ec = bad_media_format;
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
                ec = bad_media_format;
            } else {
                ec = bad_media_format;
            }
            return ec;
        }

        boost::uint32_t FlvDemuxer::probe(
            boost::uint8_t const * hbytes, 
            size_t hsize) const
        {
            if (hsize >= 3 
                && hbytes[0] == 'F'
                && hbytes[1] == 'L'
                && hbytes[2] == 'V')
            {
                return SCOPE_MAX;
            }
            return 0;
        }

        boost::uint64_t FlvDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (is_open(ec)) {
                 return timestamp().time();
            }
            return 0;
        }

        boost::uint64_t FlvDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            boost::uint64_t beg = archive_.tellg();
            archive_.seekg(0, std::ios_base::end);
            assert(archive_);
            boost::uint64_t end = archive_.tellg();
            archive_.seekg(beg, std::ios_base::beg);
            assert(archive_);
            boost::uint64_t time = 0;
            if (flv_tag_.Timestamp > timestamp_offset_ms_ + 1000) {
                time = (flv_tag_.Timestamp - timestamp_offset_ms_) * end / parse_offset_;
            } else if (metadata_.datarate) {
                time = end * 8 / metadata_.datarate;
            } else {
                time = end * 8 / 1024; // assume 1Mbps
            }
            return timestamp().const_adjust(0, timestamp_offset_ms_ + time);
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
                return ec = bad_media_format;
            } else {
                archive_.clear();
                return ec = file_stream_error;
            }
        }

    }
}

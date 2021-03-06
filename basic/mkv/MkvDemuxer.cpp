// MkvDemuxer.cpp

#include "just/demux/Common.h"
#include "just/demux/basic/mkv/MkvDemuxer.h"
#include "just/avformat/mkv/ebml/EBML_Archive.h"
#include "just/avformat/mkv/MkvAlgorithm.h"
using namespace just::demux::error;

using namespace just::avformat;
using namespace just::avformat::error;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.MkvDemuxer", framework::logger::Warn)

namespace just
{
    namespace demux
    {

        MkvDemuxer::MkvDemuxer(
            boost::asio::io_service & io_svc, 
            std::basic_streambuf<boost::uint8_t> & buf)
            : BasicDemuxer(io_svc, buf)
            , archive_(buf)
            , open_step_(size_t(-1))
            , header_offset_(0)
            , object_parse_(streams_, stream_map_)
            , buffer_parse_(streams_, stream_map_)
            , timestamp_offset_ms_(boost::uint64_t(-1))
        {
        }

        MkvDemuxer::~MkvDemuxer()
        {
        }

        error_code MkvDemuxer::open(
            error_code & ec)
        {
            open_step_ = 0;
            is_open(ec);
            return ec;
        }

        bool MkvDemuxer::is_open(
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
                std::basic_istream<boost::uint8_t> is(archive_.rdbuf());
                boost::uint64_t head_size = mkv_head_size(is, ec);

                if (ec) {
                    return false;
                }

                assert(archive_);
                archive_.seekg(0, std::ios_base::beg);
                assert(archive_);

                EBML_ElementIArchive eia(archive_, head_size);
                MkvFile file;
                MkvSegment segment;

                eia.skip(MkvCluster::StaticId);
                eia >> file >> segment;

                if (archive_) {
                    file_prop_ = segment.SegmentInfo;
                    if (file_prop_.Time_Code_Scale.empty()) {
                        file_prop_.Time_Code_Scale = 1000000;
                    }
                    for (size_t i = 0; i < segment.Tracks.Tracks.size(); ++i) {
                        MkvTrackEntry const & track = segment.Tracks.Tracks[i];
                        MkvStream stream(file_prop_, track);
                        stream.index = streams_.size();
                        if (stream_map_.size() <= (size_t)track.TrackNumber.value()) {
                            stream_map_.resize((size_t)track.TrackNumber.value() + 1, size_t(-1));
                            stream_map_[(size_t)track.TrackNumber.value()] = streams_.size();
                        }
                        streams_.push_back(stream);
                    }
                    header_offset_ = eia.skip_elements().front().offset;
                    object_parse_.reset(header_offset_);
                    open_step_ = 1;
                } else {
                    ec = bad_media_format;
                    archive_.seekg(0, std::ios_base::beg);
                }
            }

            if (open_step_ == 1) {
                if (object_parse_.ready(archive_, ec)) {
                    timestamp_offset_ms_ = object_parse_. cluster_time_code();
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        streams_[i].set_start_time(timestamp_offset_ms_);
                    }
                    object_parse_.reset(header_offset_);
                    archive_.seekg(header_offset_, std::ios_base::beg);
                    open_step_ = 2;
                    on_open();
                }
            }

            return open_step_ == 2;
        }

        bool MkvDemuxer::is_open(
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

        error_code MkvDemuxer::close(
            error_code & ec)
        {
            if (open_step_ == 2) {
                on_close();
            }
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t MkvDemuxer::seek(
            std::vector<boost::uint64_t> & dts, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            ec.clear();
            dts.assign(dts.size(), timestamp_offset_ms_);
            //current_time_ = 0;
            object_parse_.reset(header_offset_);
            return header_offset_;
        }

        boost::uint64_t MkvDemuxer::get_duration(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return just::data::invalid_size;
            }
            if (file_prop_.Duration.empty())
                return just::data::invalid_size;
            else
                return (boost::uint64_t)file_prop_.Duration.value().as_int64() * file_prop_.Time_Code_Scale.value() / 1000000;
        }

        size_t MkvDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec))
                return streams_.size();
            return 0;
        }

        error_code MkvDemuxer::get_stream_info(
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

        error_code MkvDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            if (!object_parse_.ready(archive_, ec)) {
                return ec;
            }
            if (object_parse_.itrack() >= streams_.size()) {
                LOG_WARN("[get_sample] stream index out of range: " << object_parse_.itrack());
                return get_sample(sample, ec);
            }
            MkvStream & stream = streams_[object_parse_.itrack()];
            BasicDemuxer::begin_sample(sample);
            sample.itrack = object_parse_.itrack();
            sample.flags = 0;
            if (object_parse_.is_sync_frame())
                sample.flags |= Sample::f_sync;
            if (stream.has_dts()) {
                sample.dts = stream.dts();
                stream.next();
                sample.cts_delta = (boost::uint32_t)(object_parse_.pts() - sample.dts);
                sample.duration = (boost::uint32_t)(stream.dts() - sample.dts);
            } else {
                sample.dts = object_parse_.pts();
                sample.cts_delta = 0;
                sample.duration = object_parse_.duration();
            }
            sample.size = object_parse_.size();
            sample.stream_info = &stream;
            BasicDemuxer::push_data(object_parse_.offset(), sample.size);
            object_parse_.next();
            sample.data.clear();
            for (size_t i = 0; i < stream.ContentEncodings.ContentEncodings.size(); ++i) {
                MkvContentEncoding const & encoding = stream.ContentEncodings.ContentEncodings[i];
                if (encoding.ContentEncodingType == 0) { // compression
                    switch (encoding.ContentCompression.ContentCompAlgo.value()) {
                        case 3: // header striping
                            sample.size += encoding.ContentCompression.ContentCompSettings.value().size();
                            sample.data.push_back(boost::asio::buffer(encoding.ContentCompression.ContentCompSettings.value()));
                            break;
                        default:
                            LOG_WARN("[get_sample] unsupported compression algorithm: " << encoding.ContentCompression.ContentCompAlgo.value());
                            break;
                    }
                } else {
                    LOG_WARN("[get_sample] unsupported encryption");
                }
            }
            BasicDemuxer::end_sample(sample);
            ec.clear();
            return ec;
        }

        boost::uint32_t MkvDemuxer::probe(
            boost::uint8_t const * hbytes, 
            size_t hsize)
        {
            if (hsize < 4 
                || hbytes[0] != 0x1A
                || hbytes[1] != 0x45
                || hbytes[2] != 0xDF
                || hbytes[3] != 0xA3) {
                    return 0;
            }
            size_t const len = sizeof("matroska") - 1;
            for (size_t i = 4; i + len < hsize; ++i) {
                if (memcmp(hbytes + i, "matroska", len) == 0) {
                    return SCOPE_MAX;
                }
            }
            return SCOPE_MAX / 2;
        }

        boost::uint64_t MkvDemuxer::get_cur_time(
            error_code & ec) const
        {
            if (is_open(ec)) {
                return timestamp().time();
            }
            return 0;
        }

        boost::uint64_t MkvDemuxer::get_end_time(
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            return 0;
        }

    } // namespace demux
} // namespace just

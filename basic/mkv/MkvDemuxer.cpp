// MkvDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/mkv/MkvDemuxer.h"
#include "ppbox/avformat/mkv/ebml/EBML_Archive.h"
#include "ppbox/avformat/mkv/MkvAlgorithm.h"
using namespace ppbox::demux::error;

using namespace ppbox::avformat;

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

using namespace boost::system;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.MkvDemuxer", framework::logger::Warn)

namespace ppbox
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
                    file_prop_.Time_Code_Scale = (boost::uint32_t)(1000000000 / file_prop_.Time_Code_Scale.value_or(1000000));
                    for (size_t i = 0; i < segment.Tracks.Tracks.size(); ++i) {
                        MkvTrackEntry const & track = segment.Tracks.Tracks[i];
                        MkvStream stream(track);
                        stream.index = stream_map_.size();
                        stream.time_scale = (boost::uint32_t)file_prop_.Time_Code_Scale.value();
                        stream.start_time = 0;
                        if (streams_.size() <= track.TrackNumber.value()) {
                            streams_.resize(track.TrackNumber.value() + 1);
                        }
                        streams_[track.TrackNumber.value()] = stream;
                        stream_map_.push_back(track.TrackNumber.value());
                    }
                    header_offset_ = eia.skip_elements().front().offset;
                    object_parse_.set_offset(header_offset_);
                    open_step_ = 1;
                } else {
                    ec = bad_file_format;
                    archive_.seekg(0, std::ios_base::beg);
                }
            }

            if (open_step_ == 1) {
                if (object_parse_.next_frame(archive_, ec)) {
                    boost::uint64_t start_time = object_parse_. cluster_time_code();
                    for (size_t i = 0; i < streams_.size(); ++i) {
                        streams_[i].start_time = start_time;
                    }
                    object_parse_.set_offset(header_offset_);
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
            open_step_ = size_t(-1);
            return ec = error_code();
        }

        boost::uint64_t MkvDemuxer::seek(
            boost::uint64_t & time, 
            boost::uint64_t & delta, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return 0;
            }
            ec.clear();
            time = 0;
            //current_time_ = 0;
            object_parse_ = MkvParse();
            object_parse_.set_offset(header_offset_);
            return header_offset_;
        }

        boost::uint64_t MkvDemuxer::get_duration(
            error_code & ec) const
        {
            if (!is_open(ec)) {
                return ppbox::data::invalid_size;
            }
            if (file_prop_.Duration.empty())
                return ppbox::data::invalid_size;
            else
                return (boost::uint64_t)file_prop_.Duration.value().as_int64() * 1000 / file_prop_.Time_Code_Scale.value();
        }

        size_t MkvDemuxer::get_stream_count(
            error_code & ec) const
        {
            if (is_open(ec))
                return stream_map_.size();
            return 0;
        }

        error_code MkvDemuxer::get_stream_info(
            size_t index, 
            StreamInfo & info, 
            error_code & ec) const
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

        error_code MkvDemuxer::get_sample(
            Sample & sample, 
            error_code & ec)
        {
            if (!is_open(ec)) {
                return ec;
            }
            if (!object_parse_.next_frame(archive_, ec)) {
                return ec;
            }
            if (object_parse_.track() >= streams_.size()) {
                LOG_WARN("[get_sample] stream index out of range: " << object_parse_.track());
                return get_sample(sample, ec);
            }
            MkvStream & stream = streams_[object_parse_.track()];
            BasicDemuxer::begin_sample(sample);
            sample.itrack = stream.index;
            if (object_parse_.is_sync_frame())
                sample.flags |= Sample::sync;
            sample.dts = object_parse_.dts();
            sample.cts_delta = 0;
            sample.size = object_parse_.size();
            BasicDemuxer::push_data(object_parse_.offset(), sample.size);
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

        boost::uint64_t MkvDemuxer::get_cur_time(
            error_code & ec) const
        {
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
} // namespace ppbox

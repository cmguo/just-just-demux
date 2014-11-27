// TimestampHelper.cpp

#include "just/demux/Common.h"
#include "just/demux/base/TimestampHelper.h"
#include "just/demux/base/DemuxerBase.h"

namespace just
{
    namespace demux
    {

        TimestampHelper::TimestampHelper()
            : max_delta_(500)
            , time_offset_(0)
        {
        }

        void TimestampHelper::begin(
            DemuxerBase & demuxer)
        {
            boost::system::error_code ec;
            size_t n = demuxer.get_stream_count(ec);
            if (n == 0)
                return;
            std::vector<boost::uint64_t> scale;
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                demuxer.get_stream_info(i, info, ec);
                scale.push_back(info.time_scale);
                dts.push_back(info.start_time);
            }
            set_scale(scale);
            begin(dts);
        }

        void TimestampHelper::end(
            DemuxerBase & demuxer)
        {
            boost::system::error_code ec;
            size_t n = demuxer.get_stream_count(ec);
            if (n == 0)
                return;
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                demuxer.get_stream_info(i, info, ec);
                dts.push_back(info.start_time + info.duration);
            }
            end(dts);
        }


    } // namespace demux
} // namespace just

// Demuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/Demuxer.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.Demuxer", Debug);

namespace ppbox
{
    namespace demux
    {

        Demuxer::Demuxer(
            std::basic_streambuf<boost::uint8_t> & buf)
            : helper_(&default_helper_)
        {
        }

        Demuxer::~Demuxer()
        {
        }

        void Demuxer::demux_begin(
            TimestampHelper & helper)
        {
            helper_ = &helper;
            boost::system::error_code ec;
            size_t n = get_stream_count(ec);
            if (n == 0)
                return;
            std::vector<boost::uint64_t> scale;
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                get_stream_info(i, info, ec);
                scale.push_back(info.time_scale);
                dts.push_back(info.start_time);
            }
            helper_->set_scale(scale);
            helper_->begin(dts);
        }

        void Demuxer::demux_end()
        {
            boost::system::error_code ec;
            size_t n = get_stream_count(ec);
            std::vector<boost::uint64_t> dts;
            for (size_t i = 0; i < n; ++i) {
                StreamInfo info;
                get_stream_info(i, info, ec);
                dts.push_back(info.start_time + info.duration);
            }
            helper_->end(dts);
            helper_ = &default_helper_;
        }

        void Demuxer::on_open()
        {
            demux_begin(*helper_);
        }

    } // namespace demux
} // namespace ppbox

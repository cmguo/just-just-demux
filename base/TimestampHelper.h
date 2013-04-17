// Timestamp.h

#ifndef _PPBOX_DEMUX_BASE_TIMESTAMP_H_
#define _PPBOX_DEMUX_BASE_TIMESTAMP_H_

#include <ppbox/avformat/Sample.h>

#include <framework/system/ScaleTransform.h>

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;

        class TimestampHelper
        {
        public:
            TimestampHelper();

        public:
            void set_scale(
                std::vector<boost::uint64_t> const & time_scale)
            {
                using framework::system::ScaleTransform;

                if (!time_trans_.empty()) // 只能设置一次
                    return;
                boost::uint64_t us_time_offset = time_offset_ * 1000;
                for (size_t i = 0; i < time_scale.size(); ++i) {
                    dts_offset_.push_back(ScaleTransform::static_transfer(1000, time_scale[i], time_offset_));
                    time_trans_.push_back(ScaleTransform(time_scale[i], 1000, time_offset_));
                    ustime_trans_.push_back(ScaleTransform(time_scale[i], 1000000, us_time_offset));
                }
            }

            void reset(
                boost::uint64_t time) // time miliseconds
            {
                using framework::system::ScaleTransform;

                time_offset_ = time; // 保存下来，可能set_scale后面才会调用
                boost::uint64_t us_time_offset = time_offset_ * 1000;
                for (size_t i = 0; i < dts_offset_.size(); ++i) {
                    dts_offset_[i] = ScaleTransform::static_transfer(1000, time_trans_[i].scale_in(), time_offset_);
                    time_trans_[i].reset(time_offset_);
                    ustime_trans_[i].reset(us_time_offset);
                }
            }

            // 每个分段开始的时间戳
            void begin(
                std::vector<boost::uint64_t> const & dts)
            {
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] -= dts[i];
                    time_trans_[i].last_in(time_trans_[i].last_in() + dts[i]);
                    ustime_trans_[i].last_in(ustime_trans_[i].last_in() + dts[i]);
                }
            }

            // 每个分段结束的时间戳
            void end(
                std::vector<boost::uint64_t> const & dts)
            {
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] += dts[i];
                    time_trans_[i].transfer(dts[i]);
                    time_trans_[i].last_in(0);
                    ustime_trans_[i].transfer(dts[i]);
                    ustime_trans_[i].last_in(0);
                }
            }

        public:
            void begin(
                DemuxerBase & demuxer);

            void end(
                DemuxerBase & demuxer);

        public:
            void max_delta(
                boost::uint32_t v)
            {
                max_delta_ = v;
            }

            boost::uint32_t max_delta() const
            {
                return max_delta_;
            }

        public:
            // 调整时间戳
            void adjust(
                ppbox::avformat::Sample & sample)
            {
                assert(sample.itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.discontinuity;
                }
                sample.ustime = ustime_trans_[sample.itrack].transfer(sample.dts);
                if (sample.cts_delta != (boost::uint32_t)-1) {
                    sample.us_delta = (boost::uint32_t)ustime_trans_[sample.itrack].transfer(sample.cts_delta);
                }
                sample.dts += dts_offset_[sample.itrack];
            }

            boost::uint64_t adjust(
                boost::uint32_t itrack, 
                boost::uint64_t dts)
            {
                assert(itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[itrack].transfer(dts);
                return time;
            }

            void const_adjust(
                ppbox::avformat::Sample & sample) const
            {
                assert(sample.itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.discontinuity;
                }
                sample.ustime = ustime_trans_[sample.itrack].transfer(sample.dts);
                if (sample.cts_delta != (boost::uint32_t)-1) {
                    sample.us_delta = (boost::uint32_t)ustime_trans_[sample.itrack].transfer(sample.cts_delta);
                }
                sample.dts += dts_offset_[sample.itrack];
            }

            boost::uint64_t const_adjust(
                boost::uint32_t itrack, 
                boost::uint64_t dts) const
            {
                assert(itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[itrack].transfer(dts);
                return time;
            }

            void revert(
                boost::uint64_t time, 
                std::vector<boost::uint64_t> & dts) const
            {
                for (size_t i = 0; i < time_trans_.size(); ++i) {
                    dts.push_back(time_trans_[i].revert(time));
                }
            }

        public:
            std::vector<boost::uint64_t> const & dts_offset() const
            {
                return dts_offset_;
            }

            boost::uint64_t time() const
            {
                return time_trans_.front().get();
            }

            boost::uint64_t reset_time() const
            {
                return time_offset_;
            }


        protected:
            boost::uint32_t max_delta_; // 最大帧间距离，毫秒
            boost::uint64_t time_offset_; // 毫秒
            std::vector<boost::uint64_t> dts_offset_;
            std::vector<framework::system::ScaleTransform> time_trans_; // dts -> time
            std::vector<framework::system::ScaleTransform> ustime_trans_; // dts -> ustime
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_TIMESTAMP_H_

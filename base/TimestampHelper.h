// Timestamp.h

#ifndef _PPBOX_DEMUX_BASE_TIMESTAMP_H_
#define _PPBOX_DEMUX_BASE_TIMESTAMP_H_

#include <ppbox/avformat/Sample.h>

#include <framework/system/ScaleTransform.h>

namespace ppbox
{
    namespace demux
    {

        class TimestampHelper
        {
        public:
            TimestampHelper(
                bool smoth = false)
                : smoth_(smoth)
                , smoth_begin_(false)
                , max_delta_(500)
                , time_offset_(0)
            {
            }

        public:
            void set_scale(
                std::vector<boost::uint64_t> const & time_scale)
            {
                using framework::system::ScaleTransform;

                if (!time_trans_.empty()) // 只能设置一次
                    return;
                for (size_t i = 0; i < time_scale.size(); ++i) {
                    time_trans_.push_back(ScaleTransform(time_scale[i], 1000));
                    ustime_trans_.push_back(ScaleTransform(time_scale[i], 1000000));
                    dts_offset_.push_back(ScaleTransform::static_transfer(1000, time_scale[i], time_offset_));
                }
            }

            void reset(
                boost::uint64_t time) // time miliseconds
            {
                using framework::system::ScaleTransform;

                smoth_begin_ = false;
                time_offset_ = time; // 保存下来，可能set_scale后面才会调用
                for (size_t i = 0; i < dts_offset_.size(); ++i) {
                    dts_offset_[i] = ScaleTransform::static_transfer(1000, time_trans_[i].scale_in(), time_offset_);
                }
            }

            // 每个分段开始的时间戳
            void begin(
                std::vector<boost::uint64_t> const & dts)
            {
                if (smoth_ && smoth_begin_) {
                    return;
                }
                smoth_begin_ = true;
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] -= dts[i];
                }
            }

            // 每个分段结束的时间戳
            void end(
                std::vector<boost::uint64_t> const & dts)
            {
                if (smoth_) {
                    return;
                }
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] += dts[i];
                }
            }

        public:
            void smoth(
                bool b)
            {
                smoth_ = b;
            }

            bool smoth() const
            {
                return smoth_;
            }

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
                sample.dts += dts_offset_[sample.itrack];
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.discontinuity;
                }
                sample.ustime = ustime_trans_[sample.itrack].transfer(sample.dts);
                sample.us_delta = (boost::uint32_t)ustime_trans_[sample.itrack].transfer(sample.cts_delta);
            }

            void static_adjust(
                ppbox::avformat::Sample & sample) const
            {
                assert(sample.itrack < dts_offset_.size());
                sample.dts += dts_offset_[sample.itrack];
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].static_transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.discontinuity;
                }
                sample.ustime = ustime_trans_[sample.itrack].static_transfer(sample.dts);
                sample.us_delta = (boost::uint32_t)ustime_trans_[sample.itrack].static_transfer(sample.cts_delta);
            }

        protected:
            bool smoth_;
            bool smoth_begin_; // 是否已经有一次begin
            boost::uint32_t max_delta_; // 最大帧间距离，毫秒
            boost::uint64_t time_offset_; // 毫秒
            std::vector<boost::uint64_t> dts_offset_;
            std::vector<framework::system::ScaleTransform> time_trans_; // dts -> time
            std::vector<framework::system::ScaleTransform> ustime_trans_; // dts -> ustime
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_TIMESTAMP_H_

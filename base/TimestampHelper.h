// Timestamp.h

#ifndef _PPBOX_DEMUX_BASE_TIMESTAMP_H_
#define _PPBOX_DEMUX_BASE_TIMESTAMP_H_

#include <ppbox/demux/base/DemuxBase.h>

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
                for (size_t i = 0; i < time_scale.size(); ++i) {
                    dts_offset_.push_back(ScaleTransform::static_transfer(1000, time_scale[i], time_offset_));
                    time_trans_.push_back(ScaleTransform(time_scale[i], 1000, time_offset_));
                }
            }

            void reset(
                boost::uint64_t time) // time miliseconds
            {
                using framework::system::ScaleTransform;

                time_offset_ = time; // 保存下来，可能set_scale后面才会调用
                for (size_t i = 0; i < dts_offset_.size(); ++i) {
                    dts_offset_[i] = ScaleTransform::static_transfer(1000, time_trans_[i].scale_in(), time_offset_);
                    time_trans_[i].reset(time_offset_);
                }
            }

            // 每个分段开始的时间戳
            void begin(
                std::vector<boost::uint64_t> const & dts)
            {
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] -= dts[i];
                }
            }

            // 每个分段结束的时间戳
            void end(
                std::vector<boost::uint64_t> const & dts)
            {
                assert(dts.size() == dts_offset_.size());
                for (size_t i = 0; i < dts.size(); ++i) {
                    dts_offset_[i] += dts[i];
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
                Sample & sample)
            {
                assert(sample.itrack < dts_offset_.size());
                sample.dts += dts_offset_[sample.itrack];
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.f_discontinuity;
                }
            }

            boost::uint64_t adjust(
                boost::uint32_t itrack, 
                boost::uint64_t dts)
            {
                assert(itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[itrack].transfer(dts + dts_offset_[itrack]);
                return time;
            }

            void const_adjust(
                Sample & sample) const
            {
                assert(sample.itrack < dts_offset_.size());
                sample.dts += dts_offset_[sample.itrack];
                boost::uint64_t time = time_trans_[sample.itrack].get();
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                if (time + max_delta_ < sample.time) {
                    sample.flags |= sample.f_discontinuity;
                }
            }

            boost::uint64_t const_adjust(
                boost::uint32_t itrack, 
                boost::uint64_t dts) const
            {
                assert(itrack < dts_offset_.size());
                boost::uint64_t time = time_trans_[itrack].transfer(dts + dts_offset_[itrack]);
                return time;
            }

            void revert(
                boost::uint64_t time, 
                std::vector<boost::uint64_t> & dts) const
            {
                for (size_t i = 0; i < time_trans_.size(); ++i) {
                    dts.push_back(time_trans_[i].revert(time) - dts_offset_[i]);
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
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_TIMESTAMP_H_

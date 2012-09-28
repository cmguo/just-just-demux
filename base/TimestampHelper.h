// Timestamp.h

#ifndef _PPBOX_DEMUX_BASE_TIMESTAMP_H_
#define _PPBOX_DEMUX_BASE_TIMESTAMP_H_

#include <ppbox/avformat/Format.h>

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
            {
            }

        public:
            void reset(
                std::vector<boost::uint64_t> const & time_scale, 
                boost::uint64_t time = 0) // time miliseconds
            {
                using framework::system::ScaleTransform;

                smoth_begin_ = false;
                dts_offset_.clear();
                time_trans_.clear();
                ustime_trans_.clear();
                for (size_t i = 0; i < time_scale.size(); ++i) {
                    time_trans_.push_back(ScaleTransform(time_scale[i], 1000));
                    ustime_trans_.push_back(ScaleTransform(time_scale[i], 1000000));
                    dts_offset_.push_back(ScaleTransform::static_transfer(1000, time_scale[i], time));
                }
            }

            void reset(
                boost::uint64_t time) // time miliseconds
            {
                using framework::system::ScaleTransform;

                smoth_begin_ = false;
                for (size_t i = 0; i < dts_offset_.size(); ++i) {
                    dts_offset_[i] = ScaleTransform::static_transfer(1000, time_trans_[i].scale_in(), time);
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
            bool smoth() const
            {
                return smoth_;
            }

            // 调整时间戳
            void adjust(
                ppbox::avformat::Sample & sample)
            {
                assert(sample.itrack < dts_offset_.size());
                sample.dts += dts_offset_[sample.itrack];
                sample.time = time_trans_[sample.itrack].transfer(sample.dts);
                sample.time = ustime_trans_[sample.itrack].transfer(sample.dts);
            }

        protected:
            bool smoth_;
            bool smoth_begin_; // 是否已经有一次begin
            std::vector<boost::uint64_t> dts_offset_;
            std::vector<framework::system::ScaleTransform> time_trans_; // dts -> time
            std::vector<framework::system::ScaleTransform> ustime_trans_; // dts -> ustime
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_TIMESTAMP_H_

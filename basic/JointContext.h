// JointContext.h

#ifndef _JUST_DEMUX_BASIC_JOINT_CONTEXT_H_
#define _JUST_DEMUX_BASIC_JOINT_CONTEXT_H_

#include "just/demux/base/TimestampHelper.h"

namespace just
{
    namespace demux
    {

        class BasicDemuxer;
        class JointShareInfo;
        class JointData;

        class JointContextOne
        {
        public:
            JointContextOne(
                bool smoth = false);

        public:
            void smoth(
                bool s)
            {
                smoth_ = s;
            }

            // on seek or reset
            void reset(
                boost::uint64_t time);

            // 每个分段开始时调用
            void begin(
                BasicDemuxer & demuxer);

            // 每个分段结束时调用
            void end(
                BasicDemuxer & demuxer);

        public:
            TimestampHelper & timestamp()
            {
                return timestamp_;
            }

            JointData * data() const
            {
                return data_;
            }

            void data(
                JointData * d);

            void last_time(
                boost::uint64_t t);

            boost::uint64_t last_time() const
            {
                return last_time_;
            }

        private:
            bool smoth_; // 时间戳是否平滑过渡
            bool smoth_begin_; // 是否已经有一次begin
            TimestampHelper timestamp_;
            JointData * data_;
            boost::uint64_t last_time_;
        };

        class JointContext
        {
        public:
            JointContext(
                boost::uint32_t media_flags = 0);

        public:
            // on seek or reset
            void reset(
                boost::uint64_t time);

        public:
            void media_flags(
                boost::uint32_t flags);

            boost::uint32_t media_flags() const
            {
                return media_flags_;
            }

            bool smoth() const;

            JointContextOne & read_ctx()
            {
                return read_;
            }

            JointContextOne & write_ctx()
            {
                return write_;
            }

            JointShareInfo * share_info() const
            {
                return share_info_;
            }

            void share_info(
                JointShareInfo * info);

        protected:
            boost::uint32_t media_flags_;
            JointShareInfo * share_info_;
            JointContextOne read_;
            JointContextOne write_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_JOINT_CONTEXT_H_

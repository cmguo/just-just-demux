// JointContext.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/basic/JointContext.h"
#include "ppbox/demux/basic/BasicDemuxer.h"
#include "ppbox/demux/basic/JointData.h"
#include "ppbox/demux/basic/JointShareInfo.h"

namespace ppbox
{
    namespace demux
    {

        JointContextOne::JointContextOne(
            bool smoth)
            : smoth_(smoth)
            , smoth_begin_(false)
            , data_(NULL)
            , last_time_(0)
        {
        }

        void JointContextOne::reset(
            boost::uint64_t time)
        {
            if (!smoth_ || !smoth_begin_) {
                timestamp_.reset(time);
            } else if (time < timestamp_.reset_time()) {
                smoth_begin_ = false;
                timestamp_.reset(time);
            }
            last_time_ = time;
            data(NULL);
        }

        void JointContextOne::begin(
            BasicDemuxer & demuxer)
        {
            if (smoth_ && smoth_begin_) {
                return;
            }
            smoth_begin_ = true;
            timestamp_.begin(demuxer);
        }

        void JointContextOne::end(
            BasicDemuxer & demuxer)
        {
            if (smoth_) {
                return;
            }
            timestamp_.end(demuxer);
        }

        void JointContextOne::data(
            JointData * d)
        {
            if (data_) {
                delete data_;
            }
            data_ = d;
        }

        void JointContextOne::last_time(
            boost::uint64_t t)
        {
            last_time_ = t;
        }

        // 每个分段开始的时间戳
        JointContext::JointContext(
            boost::uint32_t media_flags)
            : media_flags_(media_flags)
            , share_info_(NULL)
            , read_(smoth())
            , write_(smoth())
        {
        }

        // on seek or reset
        void JointContext::reset(
            boost::uint64_t time)
        {
            read_.reset(time);
            write_.reset(time);
        }

        void JointContext::media_flags(
            boost::uint32_t flags)
        {
            media_flags_ = flags;
            read_.smoth(smoth());
            write_.smoth(smoth());
        }

        bool JointContext::smoth() const
        {
            return (media_flags_ & MediaInfo::f_time_smoth) != 0;
        }

        void JointContext::share_info(
            JointShareInfo * info)
        {
            if (share_info_) {
                delete share_info_;
            }
            share_info_ = info;
        }

    } // namespace demux
} // namespace ppbox

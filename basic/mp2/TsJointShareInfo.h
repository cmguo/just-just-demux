// TsJointShareInfo.h

#ifndef _JUST_DEMUX_BASIC_MP2_TS_JOINT_SHARE_INFO_H_
#define _JUST_DEMUX_BASIC_MP2_TS_JOINT_SHARE_INFO_H_

#include "just/demux/basic/JointShareInfo.h"
#include "just/demux/basic/mp2/TsDemuxer.h"

namespace just
{
    namespace demux
    {

        class TsJointShareInfo
            : public JointShareInfo
        {
        public:
            TsJointShareInfo(
                TsDemuxer & demuxer)
                : streams_(demuxer.streams_)
                , stream_map_(demuxer.stream_map_)
            {
            }

            virtual ~TsJointShareInfo()
            {
            }

        public:
            std::vector<TsStream> streams_;
            std::vector<size_t> stream_map_; // Map pid to TsStream
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_MP2_TS_JOINT_SHARE_INFO_H_

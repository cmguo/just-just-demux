// TsJointShareInfo.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_JOINT_SHARE_INFO_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_JOINT_SHARE_INFO_H_

#include "ppbox/demux/basic/JointShareInfo.h"
#include "ppbox/demux/basic/ts/TsDemuxer.h"

namespace ppbox
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
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_JOINT_SHARE_INFO_H_

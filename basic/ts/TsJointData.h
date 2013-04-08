// TsJointData.h

#ifndef _PPBOX_DEMUX_BASIC_TS_TS_JOINT_DATA_H_
#define _PPBOX_DEMUX_BASIC_TS_TS_JOINT_DATA_H_

#include "ppbox/demux/basic/JointData.h"
#include "ppbox/demux/basic/ts/TsDemuxer.h"

namespace ppbox
{
    namespace demux
    {

        class TsJointData
            : public JointData
        {
        public:
            TsJointData(
                TsDemuxer & demuxer)
                : pes_parses_(demuxer.pes_parses_)
            {
            }

            virtual ~TsJointData()
            {
            }

        public:
            virtual boost::uint64_t adjust_offset(
                boost::uint64_t size)
            {
                boost::uint64_t min_off = size;
                for (size_t i = 0; i < pes_parses_.size(); ++i) {
                    if (pes_parses_[i].min_offset() < min_off) {
                        min_off = pes_parses_[i].min_offset();
                        pes_parses_[i].adjust_offset(size);
                    }
                }
                return min_off;
            }

        public:
            std::vector<PesParse> pes_parses_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_TS_TS_JOINT_DATA_H_

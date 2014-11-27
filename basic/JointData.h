// JointData.h

#ifndef _JUST_DEMUX_BASIC_JOINT_DATA_H_
#define _JUST_DEMUX_BASIC_JOINT_DATA_H_

namespace just
{
    namespace demux
    {

        class JointData
        {
        public:
            JointData()
            {
            }

            virtual ~JointData()
            {
            }

        public:
            // size: the size of previous file
            // return: the min_offset of data to be kept
            virtual boost::uint64_t adjust_offset(
                boost::uint64_t size)
            {
                return size;
            }
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_JOINT_DATA_H_

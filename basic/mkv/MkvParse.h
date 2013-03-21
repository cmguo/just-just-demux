// MkvParse.h

#ifndef _PPBOX_DEMUX_BASIC_MKV_MKV_PARSE_H_
#define _PPBOX_DEMUX_BASIC_MKV_MKV_PARSE_H_

#include <ppbox/avformat/mkv/MkvObjectType.h>

namespace ppbox
{
    namespace demux
    {

        struct MkvParse
        {
        public:
            MkvParse();

        public:
            void set_offset(
                boost::uint64_t off);

            bool next_frame(
                ppbox::avformat::EBML_IArchive & ar, 
                boost::system::error_code & ec);

        public:
            boost::uint32_t track() const
            {
                return block_.TrackNumber;
            }

            boost::uint64_t offset() const
            {
                return offset_block_ - size();
            }

            boost::uint32_t size() const
            {
                return block_.sizes[next_frame_ - 1];
            }

            boost::uint64_t dts() const
            {
                return cluster_.TimeCode.value() + block_.TimeCode;
            }

            boost::uint64_t cluster_time_code() const
            {
                return cluster_.TimeCode.value();
            }

            bool is_sync_frame() const
            {
                return block_.Keyframe == 1;
            }

        private:
            bool next_block(
                ppbox::avformat::EBML_IArchive & ar, 
                boost::system::error_code & ec);

        private:
            boost::uint64_t offset_;
            boost::uint64_t end_;
            ppbox::avformat::EBML_ElementHeader header_;
            ppbox::avformat::MkvClusterData cluster_;
            ppbox::avformat::MkvBlockData block_;
            ppbox::avformat::MkvBlockGroup group_;
            boost::uint64_t cluster_end_;
            boost::uint64_t offset_block_;
            boost::uint64_t size_block_;
            size_t next_frame_;
            bool in_group_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MKV_MKV_PARSE_H_

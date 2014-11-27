// MkvParse.h

#ifndef _JUST_DEMUX_BASIC_MKV_MKV_PARSE_H_
#define _JUST_DEMUX_BASIC_MKV_MKV_PARSE_H_

#include <just/avformat/mkv/MkvObjectType.h>

namespace just
{
    namespace demux
    {

        class MkvStream;

        struct MkvParse
        {
        public:
            MkvParse(
                std::vector<MkvStream> & streams, 
                std::vector<size_t> & stream_map);

        public:
            void reset(
                boost::uint64_t off);

            bool ready(
                just::avformat::EBML_IArchive & ar, 
                boost::system::error_code & ec);

            void next();

        public:
            boost::uint32_t itrack() const
            {
                return (boost::uint32_t)block_.TrackNumber < stream_map_.size() 
                    ? stream_map_[block_.TrackNumber] : boost::uint32_t(-1);
            }

            boost::uint64_t offset() const
            {
                return offset_block_;
            }

            boost::uint32_t size() const
            {
                return block_.sizes[frame_];
            }

            boost::uint64_t pts() const
            {
                return cluster_.TimeCode.value() + block_.TimeCode;
            }

            boost::uint32_t duration() const
            {
                return duration_;
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
                just::avformat::EBML_IArchive & ar, 
                boost::system::error_code & ec);

        private:
            std::vector<MkvStream> & streams_;
            std::vector<size_t> & stream_map_;
            boost::uint64_t offset_;
            boost::uint64_t end_;
            just::avformat::EBML_ElementHeader header_;
            just::avformat::MkvClusterData cluster_;
            just::avformat::MkvBlockData block_;
            just::avformat::MkvBlockGroup group_;
            boost::uint64_t cluster_end_;
            boost::uint64_t offset_block_;
            boost::uint32_t size_block_;
            size_t frame_;
            bool in_group_;
            boost::int16_t duration_;
        };

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_MKV_MKV_PARSE_H_

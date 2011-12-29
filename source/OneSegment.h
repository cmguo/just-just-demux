// OneSegment.h

#ifndef _PPBOX_DEMUX_ONE_SEGMENT_H_
#define _PPBOX_DEMUX_ONE_SEGMENT_H_

#include "ppbox/demux/base/SourceBase.h"

namespace ppbox
{
    namespace demux
    {

        class OneSegment
        {
        public:
            OneSegment()
            {
            }

            virtual ~OneSegment()
            {
            }

        public:
            virtual void set_name(
                std::string const & file) = 0;
        };

        template <typename Source>
        class OneSegmentT
            : public Source
            , public OneSegment
        {
        public:
            OneSegmentT(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type)
                : Source(io_svc)
                , demuxer_type_(demuxer_type)
                , segment_size_(boost::uint64_t(-1))
                , segment_time_(boost::uint64_t(-1))
                , head_size_(0)
            {
            }

        public:
            virtual DemuxerType::Enum demuxer_type() const
            {
                return demuxer_type_;
            }

            virtual size_t segment_count() const
            {
                return 1;
            }

            virtual boost::uint64_t segment_size(
                size_t segment)
            {
                return segment_size_;
            }

            virtual boost::uint64_t segment_time(
                size_t segment)
            {
                return segment_time_;
            }

            virtual boost::uint64_t segment_head_size(
                size_t segment)
            {
                return head_size_;
            }

        public:
            void set_segment_size(boost::uint64_t size)
            {
                segment_size_ = size;
            }

            void set_segment_time(boost::uint64_t time)
            {
                segment_time_ = time;
            }

            void set_head_size(boost::uint64_t head)
            {
                head_size_ = head;
            }

        private:
            DemuxerType::Enum demuxer_type_;
            boost::uint64_t segment_size_;
            boost::uint64_t segment_time_;
            boost::uint64_t head_size_;
        };

    } // namespace demux
} // namespace ppbox

#endif

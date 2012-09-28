// SegmentBuffer.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/base/DemuxStrategy.h"
#include "ppbox/demux/base/Buffer.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceError.h"

#include <ppbox/data/strategy/Strategy.h>

#include <util/event/Event.h>

#include <framework/system/LogicError.h>

namespace ppbox
{
    namespace data
    {
        class SegmentSource;
    }

    namespace demux
    {

        class BytesStream;

        class SegmentBuffer
            : public Buffer
            , private BufferObserver
        {
        public:
            typedef boost::function<void (
                boost::system::error_code const &,
                size_t)
            > prepare_response_type;

            //typedef ppbox::data::SegmentInfoEx segment_t;
            typedef ppbox::demux::SegmentPosition segment_t;

        public:
            SegmentBuffer(
                ppbox::data::SegmentSource * source, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size);

            ~SegmentBuffer();

            // Ŀǰֻ�����ڣ�seek��һ���ֶΣ���û�и÷ֶ�ͷ������ʱ��
            // ��ʱsizeΪhead_size_ͷ�����ݴ�С
            // TO BE FIXED
            boost::system::error_code seek(
                segment_t const & base,
                segment_t const & pos,
                boost::uint64_t end, 
                boost::system::error_code & ec);

            // seek���ֶεľ���λ��offset
            // TO BE FIXED
            boost::system::error_code seek(
                segment_t const & base,
                segment_t const & pos, 
                boost::system::error_code & ec);

            // Parameter: boost::uint32_t amount ��Ҫ���ص����ݴ�С
            // Parameter: boost::system::error_code & ec
            size_t prepare(
                size_t amount, 
                boost::system::error_code & ec);

            size_t prepare_at_least(
                size_t amount, 
                boost::system::error_code & ec)
            {
                return prepare(amount > prepare_size_ ? amount : prepare_size_, ec);
            }

            void async_prepare(
                size_t amount, 
                prepare_response_type const & resp);

            void async_prepare_at_least(
                size_t amount, 
                prepare_response_type const & resp)
            {
                async_prepare(amount > prepare_size_ ? amount : prepare_size_, resp);
            }

            boost::system::error_code data(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec);

            boost::system::error_code drop(
                boost::system::error_code & ec);

            /**
                drop_all 
                ������ǰ�ֶε�����ʣ�����ݣ����Ҹ��µ�ǰ�ֶ���Ϣ
             */
            boost::system::error_code drop_all(
                boost::system::error_code & ec);

            void reset(
                segment_t const & base, 
                segment_t const & pos);

        public:
            BufferStatistic stat() const
            {
                return buffer_stat();
            }

            // ��ȡ���һ�δ���Ĵ�����
            boost::system::error_code last_error() const
            {
                return last_ec_;
            }

        public:
            // д�ֶ�
            segment_t const & base_segment() const
            {
                return base_;
            }

            // ��BytesStream
            // ���ֶ�
            segment_t const & read_segment() const
            {
                return read_;
            }

            // д�ֶ�
            segment_t const & write_segment() const
            {
                return write_;
            }

            // ��BytesStream
            BytesStream & read_stream() const
            {
                return *read_stream_;
            }

            // дBytesStream
            BytesStream & write_stream() const
            {
                return *write_stream_;
            }

            // ��BytesStream
            BytesStream & bytes_stream(
                bool read) const
            {
                return read ? *read_stream_ : *write_stream_;
            }

        private:
            friend class BytesStream;

            struct PositionType
            {
                enum Enum
                {
                    set, // ��Էֶ��ļ���ʼλ��
                    beg, // �����Ч������ʼλ��
                    end, // �����Ч���ݽ���λ��
                };
            };

            boost::system::error_code segment_buffer(
                segment_t const & segment, 
                PositionType::Enum pos_type, 
                boost::uint64_t & pos, 
                boost::uint64_t & off, 
                boost::asio::const_buffer & buffer);

        private:
            virtual void on_event(
                util::event::Event const & event);

        private:
            void update_read(
                segment_t const & seg);

            void update_write(
                segment_t const & seg);

            void handle_async(
                boost::system::error_code const & ecc, 
                size_t bytes_transferred);

            void dump();

        private:
            ppbox::data::SegmentSource * source_;
            boost::uint32_t prepare_size_;  // ����һ�Σ����Ķ�ȡ���ݴ�С

            segment_t base_;
            std::deque<segment_t> segments_;
            segment_t read_;
            segment_t write_;

            BytesStream * read_stream_;    // ��Buffer
            BytesStream * write_stream_;   // дBuffer

            prepare_response_type resp_;

            boost::system::error_code last_ec_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

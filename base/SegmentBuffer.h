// SegmentBuffer.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/base/Content.h"
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

        //************************************
        // Method:    SegmentBuffer 
        // ��������
        //  1��˳�����أ�
        //  2���ṩ��ȡ�ӿڣ������ڷֶ�֮�ڣ���
        //  3�������ϱ���֧�����洦����󲢻ָ����أ�
        //  4��֧��SEEK����ȡλ�ã�дλ���Զ����������������Ѿ����ص����ݣ���
        //  5��֧�ִ������ع���������˳�򷢳�������󣩣�
        //  6���������أ�����д�ն���¼��ϢΪ���λ�ã���дָ����е�����
        //  7���ڲ���֤��д�ֶ���Ϣ����ȷ�ԣ��ṩ�ⲿʹ�ã���
        // FullName:  ppbox::demux::SegmentBuffer::SegmentBuffer
        // Access:    public 
        // Returns:   
        // Qualifier: : root_source_(source) , demuxer_(demuxer) , num_try_(size_t(-1)) , max_try_(size_t(-1)) , buffer_(NULL) , buffer_size_(framework::memory::MemoryPage::align_page(buffer_size)) , prepare_size_(prepare_size) , time_block_(0) , time_out_(0) , source_closed_(true) , data_beg_(0) , data_end_(0) , seek_end_(boost::uint64_t(-1)) , amount_(0) , expire_pause_time_(Time::now()) , total_req_(total_req) , sended_req_(0)
        // Parameter: boost::uint32_t buffer_size
        // Parameter: boost::uint32_t prepare_size
        // Parameter: Content * source
        // Parameter: BufferDemuxer * demuxer
        // Parameter: size_t total_req
        //************************************
        class SegmentBuffer
            : public Buffer
            , public BufferObserver
            , public BufferStatistic
        {
            /*
                                            offset=500
                                    segment=2   |
            |_____________|_____________|_______|______|_______________|
                         200           400     500    600             800
                                        |              |
                                    seg_beg=400     seg_end=600
            */
        public:
            typedef boost::function<void (
                boost::system::error_code const &,
                size_t)
            > prepare_response_type;

            typedef ppbox::data::SegmentInfoEx segment_t;
            //typedef ppbox::demux::SegmentPositionEx segment_t;

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

            void async_prepare(
                size_t amount, 
                prepare_response_type const & resp);

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

            // ��ȡ���һ�δ���Ĵ�����
            boost::system::error_code last_error() const
            {
                return last_ec_;
            }

        public:
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
            BytesStream * read_stream() const
            {
                return read_stream_;
            }

            // дBytesStream
            BytesStream * write_stream() const
            {
                return write_stream_;
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
                boost::uint32_t & off, 
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

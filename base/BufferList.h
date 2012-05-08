// BufferList.h

#ifndef _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_
#define _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

#include "ppbox/demux/base/SourceBase.h"
#include "ppbox/demux/base/BufferDemuxer.h"
#include "ppbox/demux/base/BufferStatistic.h"
#include "ppbox/demux/base/SourceError.h"

#include <framework/system/LogicError.h>
#include <framework/container/Array.h>
#include <framework/memory/PrivateMemory.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/timer/TimeCounter.h>

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>

#include <iostream>
#include <fstream>

namespace ppbox{
    namespace demux{
        class BytesStream;
    };
};

namespace ppbox
{
    namespace demux
    {

        //************************************
        // Method:    BufferList 
        // ��������
        //  1��˳�����أ�
        //  2���ṩ��ȡ�ӿڣ������ڷֶ�֮�ڣ���
        //  3�������ϱ���֧�����洦����󲢻ָ����أ�
        //  4��֧��SEEK����ȡλ�ã�дλ���Զ����������������Ѿ����ص����ݣ���
        //  5��֧�ִ������ع���������˳�򷢳�������󣩣�
        //  6���������أ�����д�ն���¼��ϢΪ���λ�ã���дָ����е�����
        //  7���ڲ���֤��д�ֶ���Ϣ����ȷ�ԣ��ṩ�ⲿʹ�ã���
        // FullName:  ppbox::demux::BufferList::BufferList
        // Access:    public 
        // Returns:   
        // Qualifier: : root_source_(source) , demuxer_(demuxer) , num_try_(size_t(-1)) , max_try_(size_t(-1)) , buffer_(NULL) , buffer_size_(framework::memory::MemoryPage::align_page(buffer_size)) , prepare_size_(prepare_size) , time_block_(0) , time_out_(0) , source_closed_(true) , data_beg_(0) , data_end_(0) , seek_end_(boost::uint64_t(-1)) , amount_(0) , expire_pause_time_(Time::now()) , total_req_(total_req) , sended_req_(0)
        // Parameter: boost::uint32_t buffer_size
        // Parameter: boost::uint32_t prepare_size
        // Parameter: SourceBase * source
        // Parameter: BufferDemuxer * demuxer
        // Parameter: size_t total_req
        //************************************
        class BufferList
            : public BufferObserver
            , public BufferStatistic
        {
        protected:
            FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("BufferList", 0);

        private:
            struct Hole
            {
                Hole()
                    : this_end(0)
                    , next_beg(0)
                {
                }

                boost::uint64_t this_end; // ����λ��
                boost::uint64_t next_beg;
            };

            struct Position
            {
                Position(
                    boost::uint64_t offset = 0)
                    : offset(offset)
                    , buffer(NULL)
                {
                }

                friend std::ostream & operator << (
                    std::ostream & os, 
                    Position const & p)
                {
                    os << " offset=" << p.offset;
                    os << " buffer=" << (void *)p.buffer;
                    return os;
                }

                boost::uint64_t offset; // ����ӰƬ��ƫ����
                char * buffer;          // ��ǰ�����ַ
            };
            /*
                                            offset=500
                                    segment=2   |
            |_____________|_____________|_______|______|_______________|
                         200           400     500    600             800
                                        |              |
                                    seg_beg=400     seg_end=600
            */

            struct PositionEx
                : Position
                , SegmentPositionEx
            {
                friend std::ostream & operator << (
                    std::ostream & os, 
                    PositionEx const & p)
                {
                    os << (Position const &)p;
                    os << " segment=" << p.segment;
                    os << " seg_beg=" << p.size_beg;
                    os << " seg_end=" << p.size_end;
                    return os;
                }
            };

        public:
            typedef boost::function<void (
                boost::system::error_code const &,
                size_t)
            > open_response_type;

        public:
            BufferList(
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size, 
                SourceBase * source,
                BufferDemuxer * demuxer,
                size_t total_req = 1);

            ~BufferList();

            // Ŀǰֻ�����ڣ�seek��һ���ֶΣ���û�и÷ֶ�ͷ������ʱ��
            // ��ʱsizeΪhead_size_ͷ�����ݴ�С
            // TO BE FIXED
            boost::system::error_code seek(
                SegmentPositionEx const & abs_position,
                SegmentPositionEx & position,
                boost::uint64_t offset, 
                boost::uint64_t end, 
                boost::system::error_code & ec);

            // seek���ֶεľ���λ��offset
            // TO BE FIXED
            boost::system::error_code seek(
                SegmentPositionEx const & abs_position,
                SegmentPositionEx & position, 
                boost::uint64_t offset, 
                boost::system::error_code & ec);

            void pause(
                boost::uint32_t time);

            void set_time_out(
                boost::uint32_t time_out);

            void set_max_try(
                size_t max_try);

            boost::system::error_code cancel(
                boost::system::error_code & ec);

            boost::system::error_code close(
                boost::system::error_code & ec);

            //************************************
            // Method:    prepare
            // FullName:  ppbox::demux::BufferList::prepare
            // Access:    public 
            // Returns:   boost::system::error_code
            // Qualifier:
            // Parameter: boost::uint32_t amount ��Ҫ���ص����ݴ�С
            // Parameter: boost::system::error_code & ec
            //************************************
            boost::system::error_code prepare(
                boost::uint32_t amount, 
                boost::system::error_code & ec);

            void async_prepare(
                boost::uint32_t amount, 
                open_response_type const & resp);

            void handle_async(
                boost::system::error_code const & ecc, 
                size_t bytes_transferred);

            void response(
                boost::system::error_code const & ec);

            boost::system::error_code prepare_at_least(
                boost::uint32_t amount, 
                boost::system::error_code & ec);

            void async_prepare_at_least(
                boost::uint32_t amount, 
                open_response_type const & resp);

            boost::system::error_code peek(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec);

            boost::system::error_code peek(
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec);

            //************************************
            // Method:    peek
            // FullName:  ppbox::demux::BufferList::peek
            // Access:    public 
            // Returns:   boost::system::error_code
            // Qualifier:
            // Parameter: boost::uint64_t offset ���ֶε����ƫ����
            // Parameter: boost::uint32_t size ��С
            // Parameter: std::deque<boost::asio::const_buffer> & data �������
            // Parameter: boost::system::error_code & ec ������
            //************************************
            boost::system::error_code peek(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec);

            boost::system::error_code peek(
                boost::uint32_t size, 
                std::deque<boost::asio::const_buffer> & data, 
                boost::system::error_code & ec);

            boost::system::error_code read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec);

            boost::system::error_code read(
                boost::uint32_t size, 
                std::vector<unsigned char> & data, 
                boost::system::error_code & ec);

            boost::system::error_code drop(
                boost::system::error_code & ec);

            boost::system::error_code drop_to(
                boost::uint64_t offset, 
                boost::system::error_code & ec);

            /**
                drop_all 
                ������ǰ�ֶε�����ʣ�����ݣ����Ҹ��µ�ǰ�ֶ���Ϣ
             */
            // TO BE FIXED
            boost::system::error_code drop_all(
                boost::system::error_code & ec);

            void clear();

        public:

            // ��ǰ���ֶζ�ָ��֮ǰ�Ĵ�С
            boost::uint64_t read_front() const;

            // ��ǰ���ֶ�дָ��֮ǰ�Ĵ�С
            boost::uint64_t read_back() const;

            // ��ȡָ���ֶζ�ָ��֮ǰ�Ĵ�С
            boost::uint64_t segment_read_front(
                SegmentPositionEx const & segment) const;

            // ��ȡָ���ֶ�дָ��֮ǰ�Ĵ�С
            boost::uint64_t segment_read_back(
                SegmentPositionEx const & segment) const;

            // ���ֶ�
            SegmentPositionEx const & read_segment() const
            {
                return read_;
            }

            // д�ֶ�
            SegmentPositionEx const & write_segment() const
            {
                return write_;
            }

            // ��ָ��ƫ��
            size_t read_offset() const
            {
                return read_.offset;
            }

            // дָ��ƫ��
            size_t write_offset() const
            {
                return write_.offset;
            }

            // ��BytesStream
            BytesStream * get_read_bytesstream() const
            {
                return read_bytesstream_;
            }

            // дBytesStream
            BytesStream * get_write_bytesstream() const
            {
                return write_bytesstream_;
            }

        public:
            typedef util::buffers::Buffers<
                boost::asio::const_buffer, 2
            > read_buffer_t;

            typedef util::buffers::Buffers<
                boost::asio::mutable_buffer, 2
            > write_buffer_t;

            read_buffer_t read_buffer() const
            {
                return segment_read_buffer(read_);
            }

            read_buffer_t segment_read_buffer(
                SegmentPositionEx const & segment) const;

            // ��ǰ����д����
            write_buffer_t write_buffer();

            // ��ȡд������
            // prepare����ʹ��
            write_buffer_t write_buffer(
                boost::uint32_t max_size);

            //************************************
            // Method:    add_request ��������
            // FullName:  ppbox::demux::BufferList::add_request
            // Access:    public 
            // Returns:   void
            // Qualifier:
            // Parameter: boost::system::error_code & ec
            //************************************
            void add_request(
                boost::system::error_code & ec);

            boost::uint32_t num_try() const
            {
                return num_try_;
            }

            void set_total_req(
                size_t num)
            {
                total_req_ = num;
                boost::system::error_code ec;
                open_request(true, ec);
            }

            void increase_req()
            {
                sended_req_++;
                source_closed_ = false;
            }

            void source_init();

            void insert_source(
                boost::uint64_t offset,
                SourceBase * source, 
                boost::uint64_t size,
                boost::system::error_code & ec);

            // ��ȡ���һ�δ���Ĵ�����
            boost::system::error_code last_error() const
            {
                return last_ec_;
            }

        private:
            // ����false��ʾ�����ټ�����
            bool handle_error(
                boost::system::error_code& ec);

            bool can_retry() const
            {
                return num_try_ < max_try_;
            }

            void seek_to(
                boost::uint64_t offset);

            /**
                ֻ�ڵ�ǰ�ֶ��ƶ�readָ�룬���ı�writeָ��
                ֻ���ڵ�ǰ�ֶ����ƶ�
                ��ʹ�ƶ�����ǰ�ε�ĩβ��Ҳ�����Ըı�read�ڵĵ�ǰ�ֶ�
             */
            boost::system::error_code read_seek_to(
                boost::uint64_t offset, 
                boost::system::error_code & ec);

            boost::system::error_code next_write_hole(
                PositionEx & pos, 
                Hole & hole, 
                boost::system::error_code & ec);

            void update_hole(
                PositionEx & pos,
                Hole & hole);

            void update_segments(
                boost::system::error_code & ec);

            boost::system::error_code open_request(
                bool is_next_segment,
                boost::system::error_code & ec);

            boost::system::error_code close_request(
                boost::system::error_code & ec);

            boost::system::error_code close_all_request(
                boost::system::error_code & ec);

            boost::system::error_code open_segment(
                bool is_next_segment, 
                boost::system::error_code & ec);

            void async_open_segment(
                bool is_next_segment, 
                open_response_type const & resp);

            boost::system::error_code close_segment(
                boost::system::error_code & ec);

            void dump();

            boost::uint64_t read_write_hole(
                boost::uint64_t offset, 
                Hole & hole) const;

            boost::uint64_t write_write_hole(
                boost::uint64_t offset, 
                Hole hole);

            boost::uint64_t read_read_hole(
                boost::uint64_t offset, 
                Hole & hole) const;

            boost::uint64_t write_read_hole(
                boost::uint64_t offset, 
                Hole hole);

            void read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void * dst) const;

            void read(
                boost::uint64_t offset,
                boost::uint32_t size,
                std::deque<boost::asio::const_buffer> & data);

            void write(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void const * src);

            void back_read(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void * dst) const;

            void back_write(
                boost::uint64_t offset, 
                boost::uint32_t size, 
                void const * src);

            read_buffer_t read_buffer(
                boost::uint64_t beg, 
                boost::uint64_t end) const;

            write_buffer_t write_buffer(
                boost::uint64_t beg, 
                boost::uint64_t end);

            char const * buffer_beg() const
            {
                return buffer_;
            }

            char * buffer_beg()
            {
                return buffer_;
            }

            char const * buffer_end() const
            {
                return buffer_ + buffer_size_;
            }

            char * buffer_end()
            {
                return buffer_ + buffer_size_;
            }

            // ѭ��ǰ��
            char * buffer_move_front(
                char * buffer, 
                boost::uint64_t offset) const;

            // ѭ������
            char * buffer_move_back(
                char * buffer, 
                boost::uint64_t offset) const;

            //************************************
            // Method:    move_back ����
            // FullName:  ppbox::demux::BufferList::move_back
            // Access:    private 
            // Returns:   void
            // Qualifier: const
            // Parameter: Position & position
            // Parameter: boost::uint64_t offset ��Ե�ǰλ��
            //************************************
            void move_back(
                Position & position, 
                boost::uint64_t offset) const;

            //************************************
            // Method:    move_front ǰ��
            // FullName:  ppbox::demux::BufferList::move_front
            // Access:    private 
            // Returns:   void
            // Qualifier: const
            // Parameter: Position & position
            // Parameter: boost::uint64_t offset ��Ե�ǰλ��
            //************************************
            void move_front(
                Position & position, 
                boost::uint64_t offset) const;

            //************************************
            // Method:    move_back_to ���Ƶ�
            // FullName:  ppbox::demux::BufferList::move_back_to
            // Access:    private 
            // Returns:   void
            // Qualifier: const
            // Parameter: Position & position
            // Parameter: boost::uint64_t offset �ļ�����λ��
            //************************************
            void move_back_to(
                Position & position, 
                boost::uint64_t offset) const;

            //************************************
            // Method:    move_front_to ǰ�Ƶ�
            // FullName:  ppbox::demux::BufferList::move_front_to
            // Access:    private 
            // Returns:   void
            // Qualifier: const
            // Parameter: Position & position
            // Parameter: boost::uint64_t offset �ļ�����λ��
            //************************************
            void move_front_to(
                Position & position, 
                boost::uint64_t offset) const;

            //************************************
            // Method:    move_to �ƶ���
            // FullName:  ppbox::demux::BufferList::move_to
            // Access:    private 
            // Returns:   void
            // Qualifier: const
            // Parameter: Position & position
            // Parameter: boost::uint64_t offset �ļ�����λ��
            //************************************
            void move_to(
                Position & position, 
                boost::uint64_t offset) const;

            void clear_error()
            {
                source_error_ = boost::system::error_code();
                last_ec_ = boost::system::error_code();
            }

        private:
            SourceBase * root_source_;
            BufferDemuxer * demuxer_;
            size_t num_try_;
            size_t max_try_;    // �����������������������ϵ������ı�־
            char * buffer_;
            boost::uint32_t buffer_size_;   // buffer_ ����Ĵ�С
            boost::uint32_t prepare_size_;  // ����һ�Σ����Ķ�ȡ���ݴ�С
            boost::uint32_t time_block_;    // �����������룩
            boost::uint32_t time_out_;      // ��ʱʱ�䣨�룩
            bool source_closed_;            // ture�����Ե��� open_segment(), ���� segments_->open_segment()���ʧ�ܣ�Ϊfalse
            boost::system::error_code source_error_;    // ���صĴ�����
            boost::uint64_t data_beg_;
            boost::uint64_t data_end_;
            boost::uint64_t seek_end_;  // һ����seek����ʱ�������ȡͷ�����ݣ�ֵΪ��ǰ�ֶ�֮ǰ�ķֶ��ܳ�+��ǰ�ֶε�head_size_������Ϊ-1
            PositionEx read_;
            Hole read_hole_;
            PositionEx write_;
            Hole write_hole_;

            PositionEx write_tmp_;
            Hole write_hole_tmp_;

            SegmentPositionEx abs_position_;

            BytesStream * read_bytesstream_;    // ��Buffer
            BytesStream * write_bytesstream_;   // дBuffer

            framework::memory::PrivateMemory memory_;

            boost::uint32_t amount_;    // �����첽����ʱ����ȡһ���������ݴ�С
            open_response_type resp_;
            Time expire_pause_time_;

            size_t total_req_;
            size_t sended_req_;
            boost::system::error_code last_ec_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_SOURCE_BUFFER_LIST_H_

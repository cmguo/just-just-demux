// Content.h

#ifndef _PPBOX_DEMUX_BASE_SOURCE_BASE_H_
#define _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

#include "ppbox/demux/base/DemuxerType.h"
#include "ppbox/demux/base/SourceTreeItem.h"
#include "ppbox/demux/base/SourcePrefix.h"

#include <ppbox/common/SourceBase.h>
#include <ppbox/common/SegmentBase.h>

#include <util/buffers/Buffers.h>

#include <boost/asio/buffer.hpp>

#include <map>

namespace ppbox
{
    namespace demux
    {
        struct SegmentPosition
            : SourceTreePosition
        {
            SegmentPosition()
                : segment(size_t(-1))
            {
            }

            friend bool operator == (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition const &)l == (SourceTreePosition const &)r
                    && l.segment == r.segment;
            }

            friend bool operator != (
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return (SourceTreePosition const &)l != (SourceTreePosition const &)r
                    || l.segment != r.segment;
            }

            size_t segment; // �ֶ�
        };

        struct SegmentPositionEx
            : SegmentPosition
        {
            SegmentPositionEx()
                : total_state(not_init)
                , time_state(not_init)
                , size_beg(0)
                , size_end(boost::uint64_t(-1))
                , time_beg(0)
                , time_end(boost::uint64_t(-1))
                , shard_beg(0)
                , shard_end(boost::uint64_t(-1))
            {
            }

            enum StateEnum
            {
                not_init, 
                not_exist, 
                is_valid, 
                by_guess, 

                unknown_time,
                unknown_size,
            };

            StateEnum total_state;
            StateEnum time_state;  
            boost::uint64_t size_beg; // ȫ�ֵ�ƫ��
            boost::uint64_t size_end; // ȫ�ֵ�ƫ�� 
            boost::uint64_t time_beg; // ȫ�ֵ�ƫ��
            boost::uint64_t time_end; // ȫ�ֵ�ƫ��
            boost::uint64_t shard_beg; //��Ƭ����ʼ
            boost::uint64_t shard_end; //��Ƭ�Ľ���
        };

        struct DemuxerInfo
        {
            SegmentPosition segment;
            DemuxerBase * demuxer;
            bool is_read_stream;
            boost::uint32_t ref;
        };

        struct Event
        {
            enum EventType
            {
                // ����
                EVENT_SEG_DL_OPEN,      // �ֶ����ش򿪳ɹ�
                EVENT_SEG_DL_BEGIN,     // ��ʼ���طֶ�
                EVENT_SEG_DL_END,       // �������طֶ�

                // ���װ
                EVENT_SEG_DEMUXER_OPEN, // �ֶν��װ�򿪳ɹ�
                EVENT_SEG_DEMUXER_PLAY, // ��ʼ���ŷֶ�
                EVENT_SEG_DEMUXER_STOP, // �������ŷֶ�
            };

            Event(
                EventType type,
                SegmentPositionEx seg,
                boost::system::error_code ec)
                : evt_type(type)
                , seg_info(seg)
                , ec(ec)
            {
            }

            EventType evt_type;
            SegmentPositionEx seg_info;
            boost::system::error_code ec;
        };

        class BufferList;
        class BufferDemuxer;

        // ��������:
        //  1��������Դ��
        //  2��֧��˳������ֶΣ�
        //  3���ṩͷ����С���ֶδ�С��ʱ������Ϣ����ѡ����
        //  4��֧���������λ�ã�time || size�����طֶ���Ϣ��
        //  5���򿪣�range�����رա���ȡ��ȡ�����ܣ������ṩ�ֶδ�С����򿪳ɹ����ȡ�ֶδ�С����
        //  6���ṩ�����ɾ��Դ
        class Content
            : public SourceTreeItem
        {
        public:

            static Content * create(
                boost::asio::io_service & io_svc, std::string const & playlink);

            static void destory(
                Content * sourcebase);

        public:
            Content(
                boost::asio::io_service & io_svc
                ,ppbox::common::SegmentBase* segment
                ,ppbox::common::SourceBase* source);

            ~Content();

        public:
            ppbox::common::SegmentBase* get_segment();

            ppbox::common::SourceBase* get_source();

            virtual DemuxerType::Enum demuxer_type() const = 0;


        public:
            virtual void on_error(
                boost::system::error_code & ec) {}

            // �����¼�֪ͨ
            virtual void on_event(
                Event const & evt);

        public:
            bool has_children(
                SegmentPositionEx const & position);

            virtual boost::system::error_code reset(
                SegmentPositionEx & segment) = 0;

            virtual bool next_segment(
                SegmentPositionEx & position);

            virtual boost::system::error_code time_seek (
                boost::uint64_t time, // ΢��
                SegmentPositionEx & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            virtual boost::system::error_code size_seek (
                boost::uint64_t size,  
                SegmentPositionEx const & abs_position,
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

        //private:
        public:
            // �Լ��������ӽڵ��size�ܺ�
            virtual boost::uint64_t tree_size();

            virtual boost::uint64_t source_time_before(
                size_t segment);

            // ���нڵ���child�����֮ǰ��size�ܺ�
            virtual boost::uint64_t total_size_before(
                Content * child);

            // ���нڵ���child�����֮ǰ��time�ܺ�
            virtual boost::uint64_t total_time_before(
                Content * child);

        public:
            boost::system::error_code time_insert(
                boost::uint32_t time, 
                Content * source, 
                SegmentPositionEx & position, 
                boost::system::error_code & ec);

            void update_insert(
                SegmentPositionEx const & position, 
                boost::uint32_t time, 
                boost::uint64_t offset, 
                boost::uint64_t delta);

            boost::uint64_t next_end(
                SegmentPositionEx & segment);

        public:
            void set_buffer_list(
                BufferList * buffer)
            {
                buffer_ = buffer;
            }

        public:
            boost::uint64_t insert_size() const
            {
                return insert_size_;
            }

            size_t insert_segment() const
            {
                return insert_segment_;
            }

            boost::uint64_t insert_time() const
            {
                return insert_time_;
            }

            boost::uint64_t insert_input_time() const
            {
                return insert_input_time_;
            }

            Content * parent()
            {
                return (Content *)parent_;
            }

            Content * next_sibling()
            {
                return (Content *)next_sibling_;
            }

            Content * prev_sibling()
            {
                return (Content *)prev_sibling_;
            }

            Content * first_child()
            {
                return (Content *)first_child_;
            }

            DemuxerInfo & insert_demuxer()
            {
                return insert_demuxer_;
            }

        public:

            // �Լ����зֶε�size�ܺ�
            virtual boost::uint64_t source_size();

            // ָ���ֶ�֮ǰ���зֶδ�С�ܺ�
            virtual boost::uint64_t source_size_before(
                size_t segment);

            // �Լ����зֶε�time�ܺ�
            virtual boost::uint64_t source_time();

            virtual boost::uint64_t tree_size_before(
                Content * child);

            // �Լ��������ӽڵ��time�ܺ�
            virtual boost::uint64_t tree_time();

            virtual boost::uint64_t tree_time_before(
                Content * child);

        protected:
            boost::asio::io_service & ios_service()
            {
                return io_svc_;
            }

            BufferList * buffer()
            {
                return buffer_;
            }

            BufferDemuxer * demuxer()
            {
                return demuxer_;
            }

        protected:
            SegmentPositionEx begin_segment_;

        private:
            BufferList * buffer_;
            ppbox::common::SegmentBase * segment_;
            ppbox::common::SourceBase * source_;
            boost::asio::io_service & io_svc_;
            DemuxerInfo insert_demuxer_;// ���ڵ��demuxer
            BufferDemuxer * demuxer_;
            size_t insert_segment_; // �����ڸ��ڵ�ķֶ�
            boost::uint64_t insert_size_; // �����ڷֶ��ϵ�ƫ��λ�ã�����ڷֶ���ʼλ�ã��޷���
            boost::uint64_t insert_delta_; // ��Ҫ�ظ����ص�������
            boost::uint64_t insert_time_; // �����ڷֶ��ϵ�ʱ��λ�ã�����ڷֶ���ʼλ�ã���λ��΢��
            boost::uint64_t insert_input_time_;
            std::map<std::string, SourcePrefix::Enum> type_map_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_SOURCE_BASE_H_

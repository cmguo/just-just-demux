// DemuxStrategy.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

#include "ppbox/demux/base/SourceTreeItem.h"

#include <ppbox/data/SegmentStrategy.h>

namespace ppbox
{
    namespace demux
    {

        struct SegmentPosition
            : ppbox::data::SegmentPosition
        {
            SourceTreeItem * item() const
            {
                return (SourceTreeItem *)item_context;
            }

            DemuxStrategy * owner() const
            {
                return item()->owner();
            }

            SourceTreeItem * next_item() const
            {
                return item()->next();
            }

            DemuxStrategy * next_owner() const
            {
                return next_item()->owner();
            }

            bool is_inserted() const
            {
                // next_item()->parent_ ��һ�� == this
                return next_item() != NULL && next_item()->parent()->owner() == owner();
            }
        };

        struct DemuxerInfo;

        // ��������:
        //  1��������Դ��
        //  2��֧��˳������ֶΣ�
        //  3���ṩͷ����С���ֶδ�С��ʱ������Ϣ����ѡ����
        //  4��֧���������λ�ã�time || size�����طֶ���Ϣ��
        //  5���򿪣�range�����رա���ȡ��ȡ�����ܣ������ṩ�ֶδ�С����򿪳ɹ����ȡ�ֶδ�С����
        //  6���ṩ�����ɾ��Դ
        class DemuxStrategy
            : public ppbox::data::SegmentStrategy
        {
        public:

            static DemuxStrategy * create(
                boost::asio::io_service & io_svc, 
                framework::string::Url const & playlink);

            static void destory(
                DemuxStrategy * sourcebase);

        public:
            DemuxStrategy(
                ppbox::data::MediaBase & media);

            ~DemuxStrategy();

        public:
            boost::system::error_code insert(
                SegmentPosition const & pos, 
                DemuxStrategy & child, 
                boost::system::error_code & ec);

        public:
            virtual bool next_segment(
                ppbox::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                return next_segment((SegmentPosition &)pos, ec);
            }

            //virtual bool byte_seek(
            //    boost::uint64_t offset, 
            //    ppbox::data::SegmentPosition & pos, 
            //    boost::system::error_code & ec)
            //{
            //    SegmentPosition base;
            //    return ec;
            //}

            virtual bool time_seek(
                boost::uint64_t offset, 
                ppbox::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                SegmentPosition base;
                return time_seek(offset, base, (SegmentPosition &)pos, ec);
            }

            virtual boost::uint64_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool next_segment(
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                SegmentPosition & base,
                SegmentPosition & pos, 
                boost::system::error_code & ec);

        public:
            boost::system::error_code time_insert(
                boost::uint32_t time, 
                DemuxStrategy * source, 
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            void update_insert(
                SegmentPosition const & pos, 
                boost::uint32_t time, 
                boost::uint64_t offset, 
                boost::uint64_t delta);

            boost::uint64_t next_end(
                SegmentPosition & segment);

        private:
            SourceTreeItem tree_item_;
            SourceTreeItem insert_item_;    // �����ڵ㱻�и�ĺ���һ������
            DemuxerInfo * insert_demuxer_;  // ���ڵ��demuxer
            size_t insert_segment_;         // �����ڸ��ڵ�ķֶ�
            boost::uint64_t insert_size_;   // �����ڷֶ��ϵ�ƫ��λ�ã�����ڷֶ���ʼλ�ã��޷���
            boost::uint64_t insert_delta_;  // ��Ҫ�ظ����ص�������
            boost::uint64_t insert_time_;   // �����ڷֶ��ϵ�ʱ��λ�ã�����ڷֶ���ʼλ�ã���λ��΢��
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

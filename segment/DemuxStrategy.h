// DemuxStrategy.h

#ifndef _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_
#define _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_

#include "just/demux/segment/SourceTreeItem.h"

#include <just/data/segment/SegmentStrategy.h>

namespace just
{
    namespace demux
    {

        struct DemuxerInfo;

        // ��������:
        //  1��������Դ��
        //  2��֧��˳������ֶΣ�
        //  3���ṩͷ����С���ֶδ�С��ʱ������Ϣ����ѡ����
        //  4��֧���������λ�ã�time || size�����طֶ���Ϣ��
        //  5���򿪣�range�����رա���ȡ��ȡ�����ܣ������ṩ�ֶδ�С����򿪳ɹ����ȡ�ֶδ�С����
        //  6���ṩ�����ɾ��Դ
        class DemuxStrategy
            : public just::data::SegmentStrategy
        {
        public:

            static DemuxStrategy * create(
                boost::asio::io_service & io_svc, 
                framework::string::Url const & playlink);

            static void destory(
                DemuxStrategy * sourcebase);

        public:
            DemuxStrategy(
                just::data::SegmentMedia & media);

            ~DemuxStrategy();
/*
        public:
            boost::system::error_code insert(
                just::data::SegmentPosition const & pos, 
                DemuxStrategy & child, 
                boost::system::error_code & ec);
*/
        public:
            virtual bool next_segment(
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec);

            //virtual bool byte_seek(
            //    boost::uint64_t offset, 
            //    just::data::SegmentPosition & pos, 
            //    boost::system::error_code & ec)
            //{
            //    SegmentPosition base;
            //    return ec;
            //}

            virtual bool time_seek(
                boost::uint64_t offset, 
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec)
            {
                just::data::SegmentPosition base;
                return time_seek(offset, base, pos, ec);
            }

            virtual boost::uint64_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                just::data::SegmentPosition & base,
                just::data::SegmentPosition & pos, 
                boost::system::error_code & ec);

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
} // namespace just

#endif // _JUST_DEMUX_SEGMENT_DEMUX_STRATEGY_H_

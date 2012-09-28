// DemuxStrategy.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_
#define _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

#include "ppbox/demux/base/SourceTreeItem.h"

#include <ppbox/data/MediaBase.h>
#include <ppbox/data/strategy/Strategy.h>

#include <framework/timer/TimeCounter.h>

namespace ppbox
{
    namespace demux
    {

        class DemuxerBase;

        struct SegmentPosition
            : SourceTreePosition
            , ppbox::data::SegmentInfoEx
        {
            boost::uint64_t big_time;
            boost::uint64_t time_beg;
            boost::uint64_t time_end;

            SegmentPosition()
                : big_time(0)
                , time_beg(0)
                , time_end(0)
            {
            }

            boost::uint64_t big_time_beg() const
            {
                return big_time + time_beg;
            }

            boost::uint64_t big_time_end() const
            {
                return (time_end == boost::uint64_t(-1)) 
                    ? boost::uint64_t(-1) : (big_time + time_end);
            }

            bool is_same_segment( 
                SegmentPosition const & r) const
            {
                return ((SourceTreePosition const &)(*this) == (SourceTreePosition const &)r 
                    && this->index == r.index);
            }

            friend bool operator<(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return ((SourceTreePosition const &)l < (SourceTreePosition const &)r 
                    || ((SourceTreePosition const &)l == (SourceTreePosition const &)r 
                    && (ppbox::data::SegmentInfoEx const &)l < (ppbox::data::SegmentInfoEx const &)r));
            }

            friend bool operator==(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return ((SourceTreePosition const &)l == (SourceTreePosition const &)r 
                    && (ppbox::data::SegmentInfoEx const &)l == (ppbox::data::SegmentInfoEx const &)r);
            }

            friend bool operator!=(
                SegmentPosition const & l, 
                SegmentPosition const & r)
            {
                return !(l == r);
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
            : ppbox::data::Strategy
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

            ppbox::data::MediaBase & media()
            {
                return media_;
            }

        public:
            virtual bool next_segment(
                ppbox::data::SegmentInfoEx & info)
            {
                return false;
            }

            virtual boost::system::error_code byte_seek(
                size_t offset,
                ppbox::data::SegmentInfoEx & info, 
                boost::system::error_code & ec)
            {
                return ec;
            }

            virtual boost::system::error_code time_seek(
                boost::uint32_t time_ms, 
                ppbox::data::SegmentInfoEx & info, 
                boost::system::error_code & ec)
            {
                return ec;
            }

            virtual std::size_t size(void)
            {
                return 0;
            }

        public:
            virtual bool reset(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual bool time_seek(
                boost::uint64_t time, 
                SegmentPosition & base,
                SegmentPosition & pos, 
                boost::system::error_code & ec);

            virtual bool next_segment(
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
            ppbox::data::MediaBase & media_;
            framework::timer::TimeCounter counter_; 
            DemuxerInfo * insert_demuxer_;  // ���ڵ��demuxer
            size_t insert_segment_;         // �����ڸ��ڵ�ķֶ�
            boost::uint64_t insert_size_;   // �����ڷֶ��ϵ�ƫ��λ�ã�����ڷֶ���ʼλ�ã��޷���
            boost::uint64_t insert_delta_;  // ��Ҫ�ظ����ص�������
            boost::uint64_t insert_time_;   // �����ڷֶ��ϵ�ʱ��λ�ã�����ڷֶ���ʼλ�ã���λ��΢��
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_DEMUX_STRATEGY_H_

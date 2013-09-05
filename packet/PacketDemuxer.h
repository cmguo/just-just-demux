// PacketDemuxer.h

#ifndef _PPBOX_DEMUX_PACKET_PACKET_DEMUXER_H_
#define _PPBOX_DEMUX_PACKET_PACKET_DEMUXER_H_

#include "ppbox/demux/base/Demuxer.h"
#include "ppbox/demux/packet/Filter.h"

#include <ppbox/data/packet/PacketMedia.h>
#include <ppbox/data/packet/PacketSource.h>

#include <ppbox/common/ClassFactory.h>

namespace ppbox
{
    namespace data
    {
        class PacketMedia;
        class PacketSource;
    }

    namespace demux
    {

        class PacketDemuxer
            : public Demuxer
            , public ppbox::common::ClassFactory<
                PacketDemuxer, 
                std::string, 
                PacketDemuxer * (
                    boost::asio::io_service &, 
                    ppbox::data::PacketMedia &)
                >
        {
        public:
            enum StateEnum
            {
                not_open,
                media_open,
                demuxer_open,
                open_finished,
            };

        public:
            static boost::system::error_code error_not_found();

        public:
            PacketDemuxer(
                boost::asio::io_service & io_svc, 
                ppbox::data::PacketMedia & media);

            virtual ~PacketDemuxer();

        public:
            virtual void async_open(
                open_response_type const & resp);

            virtual bool is_open(
                boost::system::error_code & ec);

            virtual boost::system::error_code cancel(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code get_media_info(
                ppbox::data::MediaInfo & info,
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

            virtual boost::uint64_t check_seek(
                boost::system::error_code & ec);

            virtual boost::system::error_code pause(
                boost::system::error_code & ec);

            virtual boost::system::error_code resume(
                boost::system::error_code & ec);

        public:
            virtual bool fill_data(
                boost::system::error_code & ec);

            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual bool free_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

            virtual bool get_data_stat(
                DataStat & stat, 
                boost::system::error_code & ec) const;

        public:
            ppbox::data::MediaBase const & media() const;

            ppbox::data::PacketSource const & source() const
            {
                return *source_;
            }

        protected:
            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec);

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

        protected:
            void add_filter(
                Filter * filter);

            void get_sample2(
                Sample & sample, 
                boost::system::error_code & ec);

            bool peek_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            void drop_sample();

        protected:
            virtual bool check_open(
                boost::system::error_code & ec);

        private:
            DemuxerBase * create_demuxer(
                boost::asio::io_service & io_svc, 
                ppbox::data::MediaBase & media);

            bool is_open(
                boost::system::error_code & ec) const;

            void handle_async_open(
                boost::system::error_code const & ecc);

            void response(
                boost::system::error_code const & ec);

        protected:
            ppbox::data::PacketMedia & media_;
            ppbox::data::PacketSource * source_;

            framework::string::Url url_;
            MediaInfo media_info_;
            std::vector<StreamInfo> stream_infos_;
            std::deque<Sample> peek_samples_;

        private:
            framework::container::List<Filter> filters_;

            boost::uint64_t seek_time_;
            bool seek_pending_;

            StateEnum open_state_;
            open_response_type resp_;
        };

    } // namespace demux
} // namespace ppbox

#define PPBOX_REGISTER_PACKET_DEMUXER(k, c) PPBOX_REGISTER_CLASS(k, c)

#endif // _PPBOX_DEMUX_PACKET_PACKET_DEMUXER_H_

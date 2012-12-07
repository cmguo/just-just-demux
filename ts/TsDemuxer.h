// TsDemuxer.h

#ifndef _PPBOX_DEMUX_TS_TS_DEMUXER_H_
#define _PPBOX_DEMUX_TS_TS_DEMUXER_H_

#include "ppbox/demux/base/Demuxer.h"

#include <ppbox/avformat/ts/PatPacket.h>
#include <ppbox/avformat/ts/PmtPacket.h>

#include <framework/system/LimitNumber.h>

namespace ppbox
{
    namespace demux
    {

        class TsStream;

        class TsDemuxer
            : public Demuxer
        {

        public:
            TsDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~TsDemuxer();

        public:
            virtual boost::system::error_code open(
                boost::system::error_code & ec);

            virtual boost::system::error_code close(
                boost::system::error_code & ec);

            virtual bool is_open(
                boost::system::error_code & ec);

        public:
            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec) const;

            virtual size_t get_stream_count(
                boost::system::error_code & ec) const;

            virtual boost::system::error_code get_stream_info(
                size_t index, 
                StreamInfo & info, 
                boost::system::error_code & ec) const;

        public:
            virtual boost::uint64_t seek(
                boost::uint64_t & time, 
                boost::uint64_t & delta, 
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) const;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

        private:
            bool is_open(
                boost::system::error_code & ec) const;

        private:
            bool get_packet(
                ppbox::avformat::TsPacket & pkt, 
                boost::system::error_code & ec);

            void skip_packet();

            bool get_pes(
                boost::system::error_code & ec);

            void free_pes();

            void free_pes(
                std::vector<ppbox::avformat::FileBlock> & payloads);

        public:
            ppbox::avformat::TsIArchive archive_;

            size_t open_step_;
            boost::uint64_t header_offset_;
            ppbox::avformat::PatProgram pat_;
            ppbox::avformat::PmtSection pmt_;
            std::vector<TsStream> streams_;
            std::vector<size_t> stream_map_; // Map pid to TsStream

            boost::uint64_t parse_offset_;
            ppbox::avformat::TsPacket pkt_;

            class PesParse;
            std::vector<PesParse> pes_parses_;
            size_t pes_index_;
            boost::uint64_t min_offset_;

            mutable boost::uint64_t parse_offset2_;
            mutable ppbox::avformat::TsPacket pkt2_;
            mutable framework::system::LimitNumber<33> time_pcr_;

            // for calc sample timestamp
            bool time_valid_;
            boost::uint64_t timestamp_offset_ms_;
            boost::uint64_t current_time_;
        };

        PPBOX_REGISTER_DEMUXER("ts", TsDemuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_TS_TS_DEMUXER_H_

// PsDemuxer.h

#ifndef _JUST_DEMUX_BASIC_MP2_PS_DEMUXER_H_
#define _JUST_DEMUX_BASIC_MP2_PS_DEMUXER_H_

#include "just/demux/basic/BasicDemuxer.h"

#include <just/avformat/mp2/PsPacket.h>
#include <just/avformat/mp2/PsmPacket.h>
#include <just/avformat/mp2/PesPacket.h>

#include <framework/system/LimitNumber.h>

namespace just
{
    namespace demux
    {

        class PsStream;
        class PesParse;
        class TsJointData;

        struct PsParse
        {
            PsParse()
                : offset(0)
            {
            }

            boost::uint64_t offset;
            just::avformat::PsPacket pkt;
            mutable framework::system::LimitNumber<33> time_pcr;
            just::avformat::PsmPacket psm;
            just::avformat::PesPacket pes;
            boost::uint64_t pes_data_offset;
            mutable framework::system::LimitNumber<33> time_pts_;
            mutable framework::system::LimitNumber<33> time_dts_;

            boost::uint64_t data_offset() const
            {
                return pes_data_offset;
            }

            boost::uint32_t size() const
            {
                return pes.payload_length();
            }

            boost::uint64_t dts() const
            {
                if (pes.PTS_DTS_flags == 3) {
                    return time_dts_.transfer(pes.dts_bits.value());
                } else if (pes.PTS_DTS_flags == 2) {
                    return time_pts_.transfer(pes.pts_bits.value());
                } else {
                    return 0;
                }
            }

            boost::uint32_t cts_delta() const
            {
                if (pes.PTS_DTS_flags == 3) {
                    return (boost::uint32_t)(time_pts_.transfer(pes.pts_bits.value()) - time_dts_.transfer(pes.dts_bits.value()));
                } else {
                    return 0;
                }
            }
        };

        class PsDemuxer
            : public BasicDemuxer
        {

        public:
            PsDemuxer(
                boost::asio::io_service & io_svc, 
                std::basic_streambuf<boost::uint8_t> & buf);

            ~PsDemuxer();

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
            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
            static boost::uint32_t probe(
                boost::uint8_t const * hbytes, 
                size_t hsize);

            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) const;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec);

        protected:
            virtual boost::uint64_t seek(
                std::vector<boost::uint64_t> & dts, 
                boost::uint64_t & delta, 
                boost::system::error_code & ec);

        private:
            bool is_open(
                boost::system::error_code & ec) const;

        private:
            bool get_packet(
                PsParse & parse, 
                boost::system::error_code & ec);

            void skip_packet(
                PsParse & parse);

            bool get_pes(
                boost::system::error_code & ec);

            void free_pes();

            void free_pes(
                std::vector<just::data::DataBlock> & payloads);

        private:
            just::avformat::Mp2IArchive archive_;

            size_t open_step_;
            boost::uint64_t header_offset_;
            just::avformat::PsSystemHeader sh_;
            std::vector<PsStream> streams_;
            std::vector<size_t> stream_map_; // Map pid to PsStream

            PsParse parse_;
            PsParse parse2_;
        };

        JUST_REGISTER_BASIC_DEMUXER("mpg", PsDemuxer);

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASIC_MP2_PS_DEMUXER_H_

// TsDemuxer.h

#ifndef _PPBOX_DEMUX_BASIC_MP2_TS_DEMUXER_H_
#define _PPBOX_DEMUX_BASIC_MP2_TS_DEMUXER_H_

#include "ppbox/demux/basic/BasicDemuxer.h"

#include <ppbox/avformat/mp2/PatPacket.h>
#include <ppbox/avformat/mp2/PmtPacket.h>

#include <framework/system/LimitNumber.h>

namespace ppbox
{
    namespace demux
    {

        class TsStream;
        class PesParse;
        class TsJointData;

        struct TsParse
        {
            TsParse()
                : offset(0)
                , had_pcr(false)
            {
            }

            boost::uint64_t offset;
            bool had_pcr;
            ppbox::avformat::TsPacket pkt;
            mutable framework::system::LimitNumber<33> time_pcr;
        };

        class TsDemuxer
            : public BasicDemuxer
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
            virtual boost::system::error_code get_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        protected:
            virtual boost::uint32_t probe(
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
            virtual JointShareInfo * joint_share();

            virtual void joint_share(
                JointShareInfo * info);

        public:
            virtual void joint_begin(
                JointContext & context);

            virtual void joint_end();

        private:
            bool is_open(
                boost::system::error_code & ec) const;

        private:
            bool get_packet(
                TsParse & parse, 
                boost::system::error_code & ec);

            void skip_packet(
                TsParse & parse);

            bool get_pes(
                boost::system::error_code & ec);

            void free_pes();

            void free_pes(
                std::vector<ppbox::data::DataBlock> & payloads);

        private:
            friend class TsJointShareInfo;
            friend class TsJointData;
            friend class TsJointData2;

            ppbox::avformat::Mp2IArchive archive_;

            size_t open_step_;
            boost::uint64_t header_offset_;
            ppbox::avformat::PatProgram pat_;
            ppbox::avformat::PmtSection pmt_;
            std::vector<TsStream> streams_;
            std::vector<size_t> stream_map_; // Map pid to TsStream

            TsParse parse_;
            TsParse parse2_;

            std::vector<PesParse> pes_parses_;
            size_t pes_index_;
        };

        PPBOX_REGISTER_BASIC_DEMUXER("ts", TsDemuxer);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP2_TS_DEMUXER_H_

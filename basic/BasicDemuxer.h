// BasicDemuxer.h

#ifndef _JUST_DEMUX_BASIC_BASIC_DEMUXER_H_
#define _JUST_DEMUX_BASIC_BASIC_DEMUXER_H_

#include "just/demux/base/Demuxer.h"

#include <just/data/base/DataBlock.h>

#include <util/tools/ClassFactory.h>

namespace just
{
    namespace demux
    {

        class JointContext;
        class JointShareInfo;

        class BasicDemuxer
            : public Demuxer
        {
        public:
            typedef std::basic_streambuf<boost::uint8_t> streambuffer_t;

        public:
            BasicDemuxer(
                boost::asio::io_service & io_svc, 
                streambuffer_t & buf);

            virtual ~BasicDemuxer();

        public:
            virtual boost::system::error_code get_media_info(
                MediaInfo & info, 
                boost::system::error_code & ec) const;

            virtual bool get_stream_status(
                StreamStatus & info, 
                boost::system::error_code & ec);

        public:
            virtual boost::system::error_code reset(
                boost::system::error_code & ec);

            virtual boost::system::error_code seek(
                boost::uint64_t & time, 
                boost::system::error_code & ec);

        public:
            virtual bool free_sample(
                Sample & sample, 
                boost::system::error_code & ec);

        public:
            virtual boost::uint64_t get_duration(
                boost::system::error_code & ec) const = 0;

            // overload by child class
            static boost::uint32_t probe(
                boost::uint8_t const * header, 
                size_t hsize);

        protected:
            virtual boost::uint64_t get_cur_time(
                boost::system::error_code & ec) const;

            virtual boost::uint64_t get_end_time(
                boost::system::error_code & ec) = 0;

        protected:
            virtual boost::uint64_t seek(
                std::vector<boost::uint64_t> & dts, 
                boost::uint64_t & delta, // 要重复下载的数据量 
                boost::system::error_code & ec) = 0;

        protected:
            virtual JointShareInfo * joint_share();

            virtual void joint_share(
                JointShareInfo * info);

        public:
            virtual void joint_begin(
                JointContext & context);

            virtual void joint_end();

            virtual void joint_begin2(
                JointContext & context);

            virtual void joint_end2();

        public:
            boost::uint64_t get_joint_cur_time(
                boost::system::error_code & ec) const;

            boost::uint64_t get_joint_end_time(
                boost::system::error_code & ec);

        protected:
            bool jointed() const
            {
                return joint_ != NULL;
            }

            JointContext & jointer()
            {
                return *joint_;
            }

        protected:
            void on_open();

            void on_close();

            void begin_sample(
                Sample & sample)
            {
                datas_.clear();
            }

            void push_data(
                boost::uint64_t off, 
                boost::uint32_t size)
            {
                datas_.push_back(just::data::DataBlock(off, size));
            }

            std::vector<just::data::DataBlock> & datas()
            {
                return datas_;
            }

            void end_sample(
                Sample & sample)
            {
                adjust_timestamp(sample);
                sample.context = &datas_;
            }

        protected:
            static boost::uint32_t const SCOPE_MAX = 100;

        private:
            streambuffer_t & buf_;
            std::vector<just::data::DataBlock> datas_;
            bool is_open_;
            JointContext * joint_;
            TimestampHelper * timestamp_;
            TimestampHelper * timestamp2_;
        };

        struct BasicDemuxerTraits
            : util::tools::ClassFactoryTraits
        {
            typedef std::string key_type;
            typedef BasicDemuxer * (create_proto)(
                boost::asio::io_service &, 
                std::basic_streambuf<boost::uint8_t> &);

            static boost::system::error_code error_not_found();
        };

        class BasicDemuxerFactory
            : public util::tools::ClassFactory<BasicDemuxerTraits>
        {
        public:
            typedef boost::uint32_t (*probe_func_t)(
                boost::uint8_t const * header,
                size_t hsize);

        public:
            static std::string probe(
                std::basic_streambuf<boost::uint8_t> & content,
                boost::system::error_code & ec);

        public:
            template <typename Demuxer>
            static void register_class(
                std::string const & key)
            {
                util::tools::ClassFactory<BasicDemuxerTraits>::register_class<Demuxer>(key);
                probe_funcs().insert(std::make_pair(key, Demuxer::probe));
            }

        private:
            typedef std::map<std::string, probe_func_t> probe_map_t;
            static probe_map_t & probe_funcs();
        };

    } // namespace demux
} // namespace just

#define JUST_REGISTER_BASIC_DEMUXER(k, c) UTIL_REGISTER_CLASS(just::demux::BasicDemuxerFactory, k, c)

#endif // _JUST_DEMUX_BASIC_BASIC_DEMUXER_H_

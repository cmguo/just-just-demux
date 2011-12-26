// PipeOneSegment.h

#include "ppbox/demux/source/PipeSource.h"
#include "ppbox/demux/source/OneSegment.h"

#include <framework/string/Parse.h>

namespace ppbox
{
    namespace demux
    {

        class PipeOneSegment
            : public OneSegmentT<PipeSource>
        {
        public:
            PipeOneSegment(
                boost::asio::io_service & io_svc, 
                DemuxerType::Enum demuxer_type)
                : OneSegmentT<PipeSource>(io_svc, pr demuxer_type)
            {
#ifndef BOOST_WINDOWS_API
                descriptor_ = ::fileno(stdin);
#else
		descriptor_ = ::GetStdHandle(STD_INPUT_HANDLE);
#endif
            }

        public:
            virtual void set_name(
                std::string const & name)
            {
                if (!name.empty())
                    descriptor_ = framework::string::parse<native_descriptor>(name);
            }

        private:
            virtual boost::system::error_code get_native_descriptor(
                size_t segment, 
                native_descriptor & descriptor, 
                boost::system::error_code & ec)
            {
                ec = boost::system::error_code();

                descriptor = descriptor_;
                return ec;
            }

        private:
            native_descriptor descriptor_;
        };

    } // namespace demux
} // namespace ppbox

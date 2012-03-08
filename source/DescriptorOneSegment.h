// DescriptorOneSegment.h

#include "ppbox/demux/source/DescriptorBufferList.h"

#include <framework/string/Parse.h>

namespace ppbox
{
    namespace demux
    {

        class DescriptorOneSegment
            : public DescriptorBufferList<DescriptorOneSegment>
        {
        public:
            DescriptorOneSegment(
                boost::asio::io_service & io_svc, 
                boost::uint32_t buffer_size, 
                boost::uint32_t prepare_size)
                : DescriptorBufferList<DescriptorOneSegment>(io_svc, buffer_size, prepare_size)
            {
#ifndef BOOST_WINDOWS_API
#  ifdef fileno
                /* Remove compile error that under mips-android platform 'fileno' is defined as a macro */
                descriptor_ = fileno(stdin);
#  else
                descriptor_ = ::fileno(stdin);
#  endif
#else
		descriptor_ = ::GetStdHandle(STD_INPUT_HANDLE);
#endif
            }

        public:
            boost::system::error_code get_native_descriptor(
                size_t segment, 
                native_descriptor & descriptor, 
                boost::system::error_code & ec)
            {
                ec = boost::system::error_code();

                descriptor = descriptor_;
                return ec;
            }

        public:
            void set_name(
                std::string const & name)
            {
                if (!name.empty())
                    descriptor_ = framework::string::parse<native_descriptor>(name);
            }

        private:
            native_descriptor descriptor_;
        };

    } // namespace demux
} // namespace ppbox

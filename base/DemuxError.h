// DemuxError.h

#ifndef _PPBOX_DEMUX_BASE_DEMUX_ERROR_H_
#define _PPBOX_DEMUX_BASE_DEMUX_ERROR_H_

#include <ppbox/avformat/Error.h>

namespace ppbox
{
    namespace demux
    {

        namespace error {

            enum errors
            {
                already_open = 1,   // demux已经打开连接
                not_open,           // demux连接未打开
            };

            namespace detail {

                class demux_category
                    : public boost::system::error_category
                {
                public:
                    demux_category()
                    {
                        register_category(*this);
                    }

                    const char* name() const
                    {
                        return "demux";
                    }

                    std::string message(int value) const
                    {
                        if (value == error::already_open)
                            return "demux: has already opened";
                        if (value == error::not_open)
                            return "demux: has not opened";
                        return "demux: unknown error";
                    }
                };

            } // namespace detail

            inline const boost::system::error_category & get_category()
            {
                static detail::demux_category instance;
                return instance;
            }

            inline boost::system::error_code make_error_code(
                errors e)
            {
                return boost::system::error_code(
                    static_cast<int>(e), get_category());
            }

        } // namespace demux_error

    } // namespace demux
} // namespace ppbox

namespace boost
{
    namespace system
    {

        template<>
        struct is_error_code_enum<ppbox::demux::error::errors>
        {
            BOOST_STATIC_CONSTANT(bool, value = true);
        };

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
        using ppbox::demux::error::make_error_code;
#endif

    }
}

#endif // _PPBOX_DEMUX_BASE_DEMUX_ERROR_H_

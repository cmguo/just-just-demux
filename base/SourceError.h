// SourceError.h

#ifndef _PPBOX_DEMUX_SOURCE_SOURCE_ERROR_H_
#define _PPBOX_DEMUX_SOURCE_SOURCE_ERROR_H_

namespace ppbox
{
    namespace demux
    {

        namespace source_error {

            enum errors
            {
                no_more_segment = 1, 
            };

            namespace detail {

                class source_category
                    : public boost::system::error_category
                {
                public:
                    source_category()
                    {
                        register_category(*this);
                    }

                    const char* name() const
                    {
                        return "source";
                    }

                    std::string message(int value) const
                    {
                        if (value == source_error::no_more_segment)
                            return "source: has no more segments";
                        return "source: unknown error";
                    }
                };

            } // namespace detail

            inline const boost::system::error_category & get_category()
            {
                static detail::source_category instance;
                return instance;
            }

            inline boost::system::error_code make_error_code(
                errors e)
            {
                return boost::system::error_code(
                    static_cast<int>(e), get_category());
            }

        } // namespace source_error

    } // namespace demux
} // namespace ppbox

namespace boost
{
    namespace system
    {

        template<>
        struct is_error_code_enum<ppbox::demux::source_error::errors>
        {
            BOOST_STATIC_CONSTANT(bool, value = true);
        };

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
        using ppbox::demux::source_error::make_error_code;
#endif

    }
}

#endif // _PPBOX_DEMUX_SOURCE_SOURCE_ERROR_H_

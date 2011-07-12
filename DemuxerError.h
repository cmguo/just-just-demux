// DemuxerError.h

#ifndef _PPBOX_DEMUX_DEMUXER_ERROR_H_
#define _PPBOX_DEMUX_DEMUXER_ERROR_H_

namespace ppbox
{
    namespace demux
    {

        namespace error {

            enum errors
            {
                already_open, // demux已经打开连接
                not_open, // demux连接未打开
                no_more_sample,           // 
                empty_name,
                bad_media_type, 
                bad_file_type, // demux连接未打开
                bad_file_format, 
                bad_smaple_order, 
                bad_offset_size, 
                file_stream_error, 
                not_support,              // 
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
                        if (value == error::no_more_sample)
                            return "demux: has no more samples";
                        if (value == error::empty_name)
                            return "demux: has empty name";
                        if (value == error::bad_media_type)
                            return "demux: bad media type";
                        if (value == error::bad_file_type)
                            return "demux: bad file type";
                        if (value == error::bad_file_format)
                            return "demux: bad file format";
                        if (value == error::bad_smaple_order)
                            return "demux: bad smaple order";
                        if (value == error::bad_offset_size)
                            return "demux: bad offset size";
                        if (value == error::file_stream_error)
                            return "demux: file stream error";
                        if (value == error::not_support)
                            return "demux: not support";
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

#endif // _PPBOX_DEMUX_DEMUXER_ERROR_H_

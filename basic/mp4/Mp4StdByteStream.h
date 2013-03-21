// Mp4StdByteStream.h

#ifndef _PPBOX_DEMUX_BASIC_MP4_MP4_STD_BYTE_STREAM_H_
#define _PPBOX_DEMUX_BASIC_MP4_MP4_STD_BYTE_STREAM_H_

#include <istream>

namespace ppbox
{
    namespace demux
    {

        class Mp4StdByteStream
            : public AP4_ByteStream
        {
        public:
            Mp4StdByteStream(
                std::basic_istream<boost::uint8_t> * is)
                : is_(* is)
                , nref_(1)
            {
            }

            virtual AP4_Result ReadPartial(
                void * buffer, 
                AP4_Size bytes_to_read, 
                AP4_Size & bytes_read)
            {
                is_.read((boost::uint8_t *) buffer, bytes_to_read);
                bytes_read = is_.gcount();
                return AP4_SUCCESS;
            }

            virtual AP4_Result WritePartial(
                const void * buffer, 
                AP4_Size bytes_to_write, 
                AP4_Size & bytes_written)
            {
                return AP4_ERROR_WRITE_FAILED;
            }

            virtual AP4_Result Seek(
                AP4_Position position)
            {
                is_.seekg(position, std::ios_base::beg);
                return AP4_SUCCESS;
            }

            virtual AP4_Result Tell(
                AP4_Position & position)
            {
                position = is_.tellg();
                return AP4_SUCCESS;
            }

            virtual AP4_Result GetSize(
                AP4_LargeSize & size)
            {
                size_t position = is_.tellg();
                is_.seekg(0, std::ios_base::end);
                size = is_.tellg();
                is_.seekg(position, std::ios_base::beg);
                return AP4_SUCCESS;
            }

            virtual AP4_Result CopyTo(
                AP4_ByteStream& stream, 
                AP4_LargeSize size)
            {
                return AP4_ERROR_READ_FAILED;
            }

            virtual void AddReference()
            {
                ++nref_;
            }

            virtual void Release()
            {
                if (--nref_ == 0) {
                    delete this;
                }
            }

        private:
            std::basic_istream<boost::uint8_t> & is_;
            size_t nref_;
        };

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASIC_MP4_MP4_STD_BYTE_STREAM_H_
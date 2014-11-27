// FFMpegDemuxer.h

#ifndef _JUST_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_
#define _JUST_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_

struct URLProtocol;

namespace just
{
    namespace data
    {
        class SingleBuffer;
    }

    namespace demux
    {

        void insert_buffer(just::data::SingleBuffer & b);

        void remove_buffer(just::data::SingleBuffer & b);

        std::string buffer_url(just::data::SingleBuffer & b);

    } // namespace demux
} // namespace just

#endif // _JUST_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_

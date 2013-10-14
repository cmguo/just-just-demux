// FFMpegDemuxer.h

#ifndef _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_
#define _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_

struct URLProtocol;

namespace ppbox
{
    namespace data
    {
        class SingleBuffer;
    }

    namespace demux
    {

        void insert_buffer(ppbox::data::SingleBuffer & b);

        void remove_buffer(ppbox::data::SingleBuffer & b);

        std::string buffer_url(ppbox::data::SingleBuffer & b);

    } // namespace demux
} // namespace ppbox

#endif // _PPBOX_DEMUX_BASE_FFMPEG_FFMPEG_PROTO_H_

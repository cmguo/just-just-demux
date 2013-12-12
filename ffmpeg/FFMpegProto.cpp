// FFMpegProto.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/ffmpeg/FFMpegProto.h"

#include <ppbox/data/single/SingleBuffer.h>
#include <ppbox/data/single/SingleSource.h>

#include <framework/string/Parse.h>
#include <framework/string/Format.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/thread/mutex.hpp>

extern "C" {

#define UINT64_C(c)   c ## ULL
#include <libavformat/avformat.h>

    typedef struct URLContext {
        const AVClass *av_class;    /**< information for av_log(). Set by url_open(). */
        struct URLProtocol *prot;
        void *priv_data;
        char *filename;             /**< specified URL */
        int flags;
        int max_packet_size;        /**< if non zero, the stream is packetized with this max packet size */
        int is_streamed;            /**< true if streamed (no seek possible), default = false */
        int is_connected;
        AVIOInterruptCB interrupt_callback;
        int64_t rw_timeout;         /**< maximum time to wait for (network) read/write operation completion, in mcs */
    } URLContext;

    typedef struct URLProtocol {
        const char *name;
        int     (*url_open)( URLContext *h, const char *url, int flags);
        int     (*url_open2)(URLContext *h, const char *url, int flags, AVDictionary **options);
        int     (*url_read)( URLContext *h, unsigned char *buf, int size);
        int     (*url_write)(URLContext *h, const unsigned char *buf, int size);
        int64_t (*url_seek)( URLContext *h, int64_t pos, int whence);
        int     (*url_close)(URLContext *h);
        struct URLProtocol *next;
        int (*url_read_pause)(URLContext *h, int pause);
        int64_t (*url_read_seek)(URLContext *h, int stream_index,
            int64_t timestamp, int flags);
        int (*url_get_file_handle)(URLContext *h);
        int (*url_get_multi_file_handle)(URLContext *h, int **handles,
            int *numhandles);
        int (*url_shutdown)(URLContext *h, int flags);
        int priv_data_size;
        const AVClass *priv_data_class;
        int flags;
        int (*url_check)(URLContext *h, int mask);
    } URLProtocol;

    int ffurl_register_protocol(URLProtocol *protocol, int size);

}

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("ppbox.demux.FFMpegProto", framework::logger::Debug);

namespace ppbox
{
    namespace demux
    {

        static void register_protocol();

        struct BufferSet
        {
            std::vector<ppbox::data::SingleBuffer *> buffers_;
            boost::mutex mutex_;
            BufferSet()
            {
                register_protocol();
            }

            void insert(ppbox::data::SingleBuffer & b)
            {
                boost::mutex::scoped_lock lc(mutex_);
                buffers_.push_back(&b);
            }

            void remove(ppbox::data::SingleBuffer & b)
            {
                boost::mutex::scoped_lock lc(mutex_);
                buffers_.erase(std::find(buffers_.begin(), buffers_.end(), &b));
            }

            ppbox::data::SingleBuffer * find(intptr_t p)
            {
                boost::mutex::scoped_lock lc(mutex_);
                std::vector<ppbox::data::SingleBuffer *>::const_iterator iter = 
                    std::find(buffers_.begin(), buffers_.end(), (ppbox::data::SingleBuffer *)p);
                if (iter == buffers_.end()) {
                    return NULL;
                } else {
                    return *iter;
                }
            }
        };

        static BufferSet & buffers()
        {
            static BufferSet s_buffers;
            return s_buffers;
        }

        static int ppbox_open(
            URLContext *h, 
            const char *url, 
            int flags)
        {
            intptr_t ptr = framework::string::parse<intptr_t>(url + 6);
            h->priv_data = buffers().find(ptr);
            if (h->priv_data) {
                return 0;
            }
            return AVERROR(ENOENT);
        }

        static int ppbox_read(
            URLContext *h, 
            unsigned char *buf, 
            int size)
        {
            ppbox::data::SingleBuffer * buffer = (ppbox::data::SingleBuffer *)h->priv_data;
            boost::system::error_code ec;
            if (buffer->Buffer::in_avail() < (size_t)size)
                buffer->prepare_some(ec);
            size = buffer->sgetn(buf, size);
            buffer->consume(size);
            if (size) {
                return size;
            }
            if (ec == boost::asio::error::would_block)
                return AVERROR(EAGAIN);
            else
                return AVERROR(ec.value());
        }

        static int64_t ppbox_seek(
            URLContext *h, 
            int64_t pos, 
            int whence)
        {
            ppbox::data::SingleBuffer * buffer = (ppbox::data::SingleBuffer *)h->priv_data;
            if (whence & AVSEEK_SIZE) {
                return buffer->source().total_size();
            }
            std::ios::seekdir const dirs[] = {std::ios::beg, std::ios::cur, std::ios::end};
            pos = buffer->pubseekoff(pos, dirs[whence], std::ios::in | std::ios::out);
            if (pos != -1) {
                return pos;
            }
            return AVERROR(EAGAIN);
        }

        static URLProtocol ff_ppbox_protocol = {
            "ppbox",            // name
            ppbox_open,         // url_open
            NULL,               // url_open2
            ppbox_read,         // url_read
            NULL,               // url_write
            ppbox_seek,         // url_seek
            NULL,               // url_close
        };

        static void register_protocol()
        {
            ffurl_register_protocol(&ff_ppbox_protocol, sizeof(URLProtocol));
        }

        void insert_buffer(ppbox::data::SingleBuffer & b)
        {
            buffers().insert(b);
        }

        void remove_buffer(ppbox::data::SingleBuffer & b)
        {
            buffers().remove(b);
        }

        std::string buffer_url(ppbox::data::SingleBuffer & b)
        {
            return "ppbox:" + framework::string::format((intptr_t)&b);
        }

    } // namespace demux
} // namespace ppbox

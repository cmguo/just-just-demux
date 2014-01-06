// FFMpegURLProtocol.h

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

// FFMpegProto.cpp

#include "just/demux/Common.h"
#include "just/demux/ffmpeg/FFMpegProto.h"
#include "just/demux/ffmpeg/FFMpegURLProtocol.h"

#include <just/data/single/SingleBuffer.h>
#include <just/data/single/SingleSource.h>

#include <framework/string/Parse.h>
#include <framework/string/Format.h>
#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>

#include <boost/thread/mutex.hpp>

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("just.demux.FFMpegProto", framework::logger::Debug);

namespace just
{
    namespace demux
    {

        static void register_protocol();

        struct BufferSet
        {
            std::vector<just::data::SingleBuffer *> buffers_;
            boost::mutex mutex_;
            BufferSet()
            {
                register_protocol();
            }

            void insert(just::data::SingleBuffer & b)
            {
                boost::mutex::scoped_lock lc(mutex_);
                buffers_.push_back(&b);
            }

            void remove(just::data::SingleBuffer & b)
            {
                boost::mutex::scoped_lock lc(mutex_);
                buffers_.erase(std::find(buffers_.begin(), buffers_.end(), &b));
            }

            just::data::SingleBuffer * find(intptr_t p)
            {
                boost::mutex::scoped_lock lc(mutex_);
                std::vector<just::data::SingleBuffer *>::const_iterator iter = 
                    std::find(buffers_.begin(), buffers_.end(), (just::data::SingleBuffer *)p);
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

        static int just_open(
            URLContext *h, 
            const char *url, 
            int flags)
        {
            intptr_t ptr = framework::string::parse<intptr_t>(url + 5);
            h->priv_data = buffers().find(ptr);
            if (h->priv_data) {
                return 0;
            }
            return AVERROR(ENOENT);
        }

        static int just_read(
            URLContext *h, 
            unsigned char *buf, 
            int size)
        {
            just::data::SingleBuffer * buffer = (just::data::SingleBuffer *)h->priv_data;
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

        static int64_t just_seek(
            URLContext *h, 
            int64_t pos, 
            int whence)
        {
            just::data::SingleBuffer * buffer = (just::data::SingleBuffer *)h->priv_data;
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

        static URLProtocol ff_just_protocol = {
            "just",            // name
            just_open,         // url_open
            NULL,               // url_open2
            just_read,         // url_read
            NULL,               // url_write
            just_seek,         // url_seek
            NULL,               // url_close
        };

        static void register_protocol()
        {
            ffurl_register_protocol(&ff_just_protocol, sizeof(URLProtocol));
        }

        void insert_buffer(just::data::SingleBuffer & b)
        {
            buffers().insert(b);
        }

        void remove_buffer(just::data::SingleBuffer & b)
        {
            buffers().remove(b);
        }

        std::string buffer_url(just::data::SingleBuffer & b)
        {
            return "just:" + framework::string::format((intptr_t)&b);
        }

    } // namespace demux
} // namespace just

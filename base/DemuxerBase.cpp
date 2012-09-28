// DemuxerBase.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/base/DemuxerBase.h"

#include <framework/logger/Logger.h>
#include <framework/logger/StreamRecord.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("DemuxerBase", 0);

namespace ppbox
{
    namespace demux
    {

        std::map<std::string, DemuxerBase::register_type> & DemuxerBase::demuxer_map()
        {
            static std::map<std::string, DemuxerBase::register_type> g_map;
            return g_map;
        }

        void DemuxerBase::register_demuxer(
            std::string const & format,
            register_type func)
        {
            demuxer_map().insert(std::make_pair(format, func));
            return;
        }

        DemuxerBase * DemuxerBase::create(
            std::string const & format, 
            std::basic_streambuf<boost::uint8_t> & buf)
        {
            std::map<std::string, register_type>::iterator iter = 
                demuxer_map().find(format);
            if (demuxer_map().end() == iter) {
                return NULL;
            } else {
                register_type func = iter->second;
                DemuxerBase * demuxer = func(buf);
                return demuxer;
            }
        }

        void DemuxerBase::destory(
            DemuxerBase* & demuxer)
        {
            delete demuxer;
            demuxer = NULL;
        }

    } // namespace demux
} // namespace ppbox

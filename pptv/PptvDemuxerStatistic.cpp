// PptvDemuxerStatistic.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/PptvDemuxerStatistic.h"

#include <framework/logger/Logger.h>
#include <framework/logger/LoggerStreamRecord.h>
#include <framework/logger/LoggerSection.h>
using namespace framework::logger;

FRAMEWORK_LOGGER_DECLARE_MODULE_LEVEL("PptvDemuxerStatistic", 0);

namespace ppbox
{
    namespace demux
    {

        PptvDemuxerStatistic::PptvDemuxerStatistic()
            : play_type_(PptvDemuxerType::none)
            , open_total_time_(0)
            , is_ready_(true)
        {
        }

    }
}

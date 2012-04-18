// CommonDemuxer.cpp

#include "ppbox/demux/Common.h"
#include "ppbox/demux/CommonDemuxer.h"
#include "ppbox/demux/pptv/PptvDemuxer.h"
#include "ppbox/demux/source/SourceType.h"
#include "ppbox/demux/source/FileOneSegment.h"
#include "ppbox/demux/source/HttpOneSegment.h"
#include "ppbox/demux/source/PipeOneSegment.h"

namespace ppbox
{
    namespace demux
    {

        BufferDemuxer * CommonDemuxer::create(
            util::daemon::Daemon & daemon,
            std::string const & url_str,
            boost::uint32_t buffer_size,
            boost::uint32_t prepare_size)
        {
            //static std::map<std::string, SourceType::Enum> type_map;
            //if (type_map.empty()) {
            //    type_map["http"] = SourceType::http;
            //    type_map["file"] = SourceType::file; //file:///fff.ts
            //    type_map["pipe"] = SourceType::pipe; //pipe:///88.flv
            //}
            //framework::string::Url url(url_str);
            //SourceType::Enum source_type = SourceType::none;
            //std::map<std::string, SourceType::Enum>::const_iterator iter = 
            //    type_map.find(url.protocol());
            //if (iter != type_map.end()) {
            //    source_type = iter->second;
            //}
            //if (source_type == SourceType::none && url.protocol().find("pp") == 0) {
            //    return PptvDemuxer::create(daemon, url, buffer_size, prepare_size);
            //}
            //static std::map<std::string, DemuxerType::Enum> demuxer_map;
            //if (demuxer_map.empty()) {
            //    demuxer_map["mp4"] = DemuxerType::mp4;
            //    demuxer_map["asf"] = DemuxerType::asf;
            //    demuxer_map["flv"] = DemuxerType::flv;
            //    demuxer_map["ts"] = DemuxerType::ts;
            //}
            //std::string exten_str = url_str.substr(url_str.rfind('.') + 1);
            //DemuxerType::Enum demuxer_type = DemuxerType::none;
            //std::map<std::string, DemuxerType::Enum>::const_iterator demuxer_iter = 
            //    demuxer_map.find(exten_str);
            //if (demuxer_iter != demuxer_map.end()) {
            //    demuxer_type = demuxer_iter->second;
            //}
            //switch (source_type) {
            //    case SourceType::http:
            //        return new CommonDemuxer(daemon.io_svc(), buffer_size, prepare_size, 
            //            new HttpOneSegment(daemon.io_svc(), demuxer_type), url_str) ;
            //    case SourceType::file:
            //        return new CommonDemuxer(daemon.io_svc(), buffer_size, prepare_size, 
            //            new FileOneSegment(daemon.io_svc(), demuxer_type), url_str);
            //    case SourceType::pipe:
            //        return new CommonDemuxer(daemon.io_svc(), buffer_size, prepare_size, 
            //            new PipeOneSegment(daemon.io_svc(), demuxer_type), url_str);
            //    default:
            //        assert(0);
            //        return NULL;
            //}
            return NULL;
        }

        //boost::system::error_code CommonDemuxer::open(
        //    std::string const & name, 
        //    boost::system::error_code & ec)
        //{
        //    /*std::vector<std::string> key_playlink;
        //    slice<std::string>(name, std::inserter(
        //        key_playlink, key_playlink.end()), "|");
        //    assert(key_playlink.size() > 0);
        //    std::string playlink = key_playlink[key_playlink.size()-1];*/
        //    //source_->set_name(url_str_);
        //    //return BufferDemuxer::open(name, ec);
        //}

        //void CommonDemuxer::async_open(
        //    std::string const & name, 
        //    BufferDemuxer::open_response_type const & resp)
        //{
        //    /*std::vector<std::string> key_playlink;
        //    slice<std::string>(name, std::inserter(
        //        key_playlink, key_playlink.end()), "|");
        //    assert(key_playlink.size() > 0);
        //    std::string playlink = key_playlink[key_playlink.size()-1];*/
        //    source_->set_name(url_str_);
        //    BufferDemuxer::async_open(name, resp);
        //}

    } // namespace demux
} // namespace ppbox

//
//  WsImpl.cpp
//  mcu
//
//  Created by sunkw on 14-5-7.
//  Copyright (c) 2014年 sunkw. All rights reserved.
//

#include "WsImpl.h"
#include "const.h"
#include "util.h"
#include "log.h"
#include "webapi.h"

int mcu__getVersion(soap *soap, void *, mcu__VersionResponse &res)
{
    res.major = VERSION_MAJOR;
    res.minor = VERSION_MINOR;
    res.desc = soap_strdup(soap, VERSION_DESC);
    
    return SOAP_OK;
}

int mcu__getStatus(soap *soap, void *, mcu__StatusResponse &res)
{
    Server &server = get_server(soap);
    
    res.cpu = 0.0;
    res.mem = 0.0;
    
    res.conferences = (int)server.get_conference_count();
    
    res.down_lost_ratio = 0.0;
    res.down_kbps5 = 0.0;
    res.down_kbps60 = 0.0;
    res.down_kbps300 = 0.0;
    
    res.up_lost_ratio = 0.0;
    res.up_kbps5 = 0.0;
    res.up_kbps60 = 0.0;
    res.up_kbps300 = 0.0;
    
    res.uptime = util_uptime();
    
    return SOAP_OK;
}

int mcu__addVideoSource(soap *soap, int conf_id, int member_id, mcu__VideoSourceResponse &res)
{
    return SOAP_OK;
}

int mcu__delVideoSource(soap *soap, int conf_id, int member_id, int source_id)
{
    return SOAP_OK;
}

int mcu__addVideoSink(soap *soap, int conf_id, int member_id, int source_id, mcu__VideoSinkResponse &res)
{
    return SOAP_OK;
}

int mcu__delVideoSink(soap *soap, int conf_id, int member_id, int sink_id)
{
    return SOAP_OK;
}

int mcu__addAudioStream(soap *soap, int conf_id, int member_id, mcu__AudioStreamResponse &res)
{
    return SOAP_OK;
}

int mcu__delAudioStream(soap *soap, int conf_id, int member_id, int streamid)
{
    return SOAP_OK;
}

int mcu__addMember(soap *soap, int conf_id, mcu__MemberResponse &res)
{
    Server &server = get_server(soap);
    Conference *conf = server.get_conference(conf_id);
    if (conf) {
        // TODO: ...
        
        conf->release();
    }
    
    return SOAP_OK;
}

int mcu__delMember(soap *soap, int conf_id, int member_id)
{
    return SOAP_OK;
}

int mcu__enableSound(soap *soap, int conf_id, int member_id, int enable, int &res)
{
    return SOAP_OK;
}

int mcu__addConference(soap *soap, void *, mcu__ConferenceResponse &res)
{
    Server &server = get_server(soap);
    
    res.confid = server.conference_create();
    res.desc = soap_strdup(soap, "");
    res.members.__ptr = 0;      // 创建时，总是空
    res.members.__size = 0;
    
    return SOAP_OK;
}

int mcu__delConference(soap *soap, int conf_id)
{
    Server &server = get_server(soap);
    server.conference_destroy(conf_id);
    
    return SOAP_OK;
}

int mcu__listConferences(soap *soap, void *, mcu__ConferenceArrayResponse &res)
{
    Server &server = get_server(soap);
    std::vector<ConferenceDesc> confdescs = server.conference_list();
    
    res.__ptr = 0;
    res.__size = (int)confdescs.size();
    if (res.__size > 0) {
        res.__ptr = (struct mcu__Conference*)soap_malloc(soap, sizeof(struct mcu__Conference));
        
        int n = 0;
        for (std::vector<ConferenceDesc>::iterator it = confdescs.begin(); it != confdescs.end(); ++it) {
            // TODO: 填充
            struct mcu__Conference *conf = &res.__ptr[n++];
            
            conf->confid = it->conf_id;
        }
    }
    
    return SOAP_OK;
}

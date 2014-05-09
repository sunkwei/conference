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
    return SOAP_OK;
}

int mcu__delConference(soap *soap, int conf_id)
{
    return SOAP_OK;
}

int mcu__listConferences(soap *soap, void *, mcu__ConferenceArrayResponse &res)
{
    return SOAP_OK;
}

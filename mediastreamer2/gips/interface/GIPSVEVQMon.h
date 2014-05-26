// GIPSVEVQMon.h
//
// Copyright (c) 2008 Global IP Solutions. All rights reserved.

#if !defined(__GIPSVE_VQ_MON_H__)
#define __GIPSVE_VQ_MON_H__

#include "GIPSVECommon.h"

class GIPSVoiceEngine;

class VOICEENGINE_DLLEXPORT GIPSVEVQMonCallback
{
public:
    virtual void OnVQMonAlert(unsigned int instance, int channel, void* alertDesc) = 0;
protected:
    virtual ~GIPSVEVQMonCallback() { }
};

class VOICEENGINE_DLLEXPORT GIPSVEVQMon
{
public:
    static GIPSVEVQMon* GetInterface(GIPSVoiceEngine* voiceEngine);

    // Enable/disable VQMon and RTCP-XR
    virtual int GIPSVE_SetVQMonStatus(int channel, bool enable) = 0;
    virtual int GIPSVE_SetRTCPXRStatus(int channel, bool enable) = 0;

    // Alert handling
    virtual int GIPSVE_SetVQMonAlertCallback(GIPSVEVQMonCallback* vqmonCallback) = 0;
    virtual int GIPSVE_EnableVQMonAlert(int channel, int type, int param1[4], int param2[4], int param3[4]) = 0;
    virtual int GIPSVE_DisableVQMonAlert(int channel, int type) = 0;

    // Get info
    virtual int GIPSVE_GetVoipMetrics(int channel, unsigned char* data, unsigned int& length) = 0;
    virtual int GIPSVE_GetVQMonSIPReport(int channel, unsigned char* data, unsigned int& length,
        char strSIPLocalCallID[80], char strSIPRemoteCallID[80], char strSIPLocalStartTimestamp[30],
        char strSIPLocalStopTimestamp[30], char strSIPRemoteStartTimestamp[30], char strSIPRemoteStopTimestamp[30]) = 0;

    // Set IP info
    virtual int GIPSVE_SetVQMonIPInfo(int channel, const unsigned char* localIP, int localPort, const unsigned char* remoteIP, int remotePort) = 0;

    virtual int Release() = 0;
protected:
    virtual ~GIPSVEVQMon();
};

#endif    // #if !defined(__GIPSVE_VQ_MON_H__)

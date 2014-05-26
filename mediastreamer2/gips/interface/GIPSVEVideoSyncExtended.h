// GIPSVEVideoSyncExtended.h
//
// Copyright (c) 1999-2008 Global IP Solutions. All rights reserved.

#if !defined(__GIPSVE_VIDEO_SYNC_EXTENDED_H__)
#define __GIPSVE_VIDEO_SYNC_EXTENDED_H__

#include "GIPSVECommon.h"

class GIPSVoiceEngine;
class GIPSModuleRtpRtcp;

class VOICEENGINE_DLLEXPORT GIPSVEVideoSyncExtended
{
public:
    static GIPSVEVideoSyncExtended* GetInterface(GIPSVoiceEngine* voiceEngine);

    virtual int GIPSVE_GetGIPSModuleRtpRtcp (int channel, GIPSModuleRtpRtcp* &rtpRtcpModule) = 0;

    virtual int Release() = 0;

protected:
    virtual ~GIPSVEVideoSyncExtended();
};

#endif  // #if !defined(__GIPSVE_VIDEO_SYNC_EXTENDED_H__)

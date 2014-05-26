// GIPSVEG729Extended.h
//
// Copyright (c) 1999-2008 Global IP Solutions. All rights reserved.

#if !defined(__GIPSVE_G729_EXTENDED_H__)
#define __GIPSVE_G729_EXTENDED_H__

#include "GIPSVECommon.h"

class GIPSVoiceEngine;

class VOICEENGINE_DLLEXPORT GIPSVEG729Extended
{
public:
    static GIPSVEG729Extended* GetInterface(GIPSVoiceEngine* voiceEngine);

    // G.729 Annex B control
    virtual int GIPSVE_SetG729AnnexBStatus(int channel, bool enable) = 0;
    virtual int GIPSVE_GetG729AnnexBStatus(int channel, bool& enabled) = 0;

    // Threshold control
    virtual int GIPSVE_SetG729SidResendThreshold(int channel, short sidThreshold) = 0;

	/* THIS FUNCTION IS OBSOLETE AND WILL BE REMOVED IN THE NEXT VE VERSION */
	virtual int GIPSVE_SetG729SilenceThreshold(int channel, short silenceThreshold) = 0;

    virtual int Release() = 0;

protected:
    virtual ~GIPSVEG729Extended();
};

#endif    // #if !defined(__GIPSVE_G729_EXTENDED_H__)

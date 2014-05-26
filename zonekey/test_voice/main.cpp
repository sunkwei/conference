#include <stdio.h>
#include <stdlib.h>
#include <GIPSVEBase.h>
#include <GIPSVEVQE.h>
#include <WinSock2.h>
#include "../../gips/detours/src/detours.h"

#pragma comment(lib, "../../gips/lib/GIPSVoiceEngineLib_MT.lib")
#pragma comment(lib, "avrt.lib")

void (__stdcall * Real_GetSystemTime)(LPSYSTEMTIME a0) = GetSystemTime;

static void __stdcall Mine_GetSystemTime(LPSYSTEMTIME a0)
{
    Real_GetSystemTime(a0);
	a0->wYear = 2010;
	a0->wMonth = 4;
}

namespace {
	class Init
	{
	public:
		Init()
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)Real_GetSystemTime, Mine_GetSystemTime);
			DetourTransactionCommit();
		}
	};
};

static Init _init;

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <remote ip>\n", argv[0]);
		return -1;
	}

	GIPSVoiceEngine *engine = GIPSVoiceEngine::Create();
	GIPSVEBase *vebase = GIPSVEBase::GIPSVE_GetInterface(engine);
	GIPSVEVQE *vevqe = GIPSVEVQE::GetInterface(engine);

	int rc = vebase->GIPSVE_Init(5, 15, 2010);
	if (rc < 0) {
		fprintf(stderr, "ERR: GIPSVE_Init err\n");
		return -1;
	}

	rc = vevqe->GIPSVE_SetECStatus(true, EC_DEFAULT);
	fprintf(stderr, "INFO: SetECStatus ret %d\n", rc);

	rc = vevqe->GIPSVE_SetAGCStatus(false);

	int ch = vebase->GIPSVE_CreateChannel();
	fprintf(stderr, "INFO: CreateChannel ret %d\n", ch);

	vebase->GIPSVE_SetLocalReceiver(ch, 40000);
	fprintf(stderr, "INFO: using local port 40000\n");

	vebase->GIPSVE_StartListen(ch);
	fprintf(stderr, "INFO: listen 40000\n");


	vebase->GIPSVE_SetSendDestination(ch, 40000, argv[1]);
	fprintf(stderr, "INFO: remote addr=%s:%d\n", argv[1], 40000);

	vebase->GIPSVE_StartPlayout(ch);
	vebase->GIPSVE_StartSend(ch);

	for (; ; ) {
		Sleep(1000);
		fprintf(stderr, ".");
	}

	return 0;
}

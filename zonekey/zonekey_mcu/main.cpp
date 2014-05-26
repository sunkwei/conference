#undef UNICODE

/// 检查授权
#define LICENSE 0

#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <mediastreamer2/zk.publisher.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/zk.audio.mixer.h>
#include <mediastreamer2/zk.void.h>
#include "Server.h"
#include <algorithm>
#include <vector>
#if LICENSE
#  include <e-license.h>

#	pragma comment(lib, "libzklcs.lib")
#endif // 

static int _quit = 0;
#ifdef WIN32
#	define GLOBAL_NAME "Global\\zonekey_mcu"
#	define SERVICE_NAME "zonekey.service.mcu"

static HANDLE _global_obj = 0;
static bool _verbase = false;

GlobalConfig *_gc = 0;

static int install_service(const char *appname)
{
	fprintf(stdout, "..%s..\n", __FUNCTION__);
	SC_HANDLE sm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sm) {
		fprintf(stderr, "ERROR: %s: OpenSCManager err\n", __FUNCTION__);
		return -1;
	}

	char name[1024];
	GetModuleFileName(0, name, sizeof(name));

	SC_HANDLE sh = CreateService(sm, SERVICE_NAME, SERVICE_NAME, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, 
		SERVICE_ERROR_NORMAL, name, 0, 0, 0, 0, 0);
	if (!sh) {
		fprintf(stderr, "ERROR: %s: CreateService for %s, using %s, err\n", __FUNCTION__, SERVICE_NAME, name);
		CloseServiceHandle(sm);
		return -1;
	}
	fprintf(stdout, "OK: %s (%s) installed successful!\n", SERVICE_NAME, name);
	CloseServiceHandle(sh);
	CloseServiceHandle(sm);
	return 0;
}

static int uninstall_service(const char *appname)
{
	fprintf(stdout, "..%s..\n", __FUNCTION__);
	SC_HANDLE sm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sm) {
		fprintf(stderr, "ERROR: %s: OpenSCManager err\n", __FUNCTION__);
		return -1;
	}
	SC_HANDLE sh = OpenService(sm, SERVICE_NAME, DELETE);
	if (!sh) {
		fprintf(stderr, "ERROR: %s: OpenService for DELETE err\n", __FUNCTION__);
		CloseServiceHandle(sm);
		return -1;
	}
	if (!DeleteService(sh)) {
		fprintf(stderr, "ERROR: %s: DeleteService err\n", __FUNCTION__);
		CloseServiceHandle(sh);
		CloseServiceHandle(sm);
		return -1;
	}
	fprintf(stdout, "OK: %s removed successful!\n", SERVICE_NAME);
	CloseServiceHandle(sh);
	CloseServiceHandle(sm);

	return 0;
}
#else
# define WINAPI
#endif // WIN32

#if LICENSE

typedef std::map<std::string, std::string> KVS;
static KVS parse_feathers(const char *feathers)
{
	KVS fs;
	if (!feathers) return fs;

	char *str = strdup(feathers);

	char *p = strtok(str, "&");
	while (p) {
		char key[64], value[256];
		if (sscanf(p, "%63[^=] = %255[^\r\n]", key, value) == 2) {
			fs[key] = value;
		}

		p = strtok(0, "&");
	}

	free(str);
	return fs;
}

#endif 

static void WINAPI mainp(int argc, char **argv);

int main(int argc, char **argv)
{
	_gc = new GlobalConfig;

#ifdef WIN32
	_verbase = false;
	if (argc == 2 && strcmp(argv[1], "/install") == 0) {
		return install_service(argv[0]);
	}
	if (argc == 2 && strcmp(argv[1], "/uninstall") == 0) {
		return uninstall_service(argv[0]);
	}
	if (argc == 2 && strcmp(argv[1], "/debug") == 0) {
		_verbase = true;
	}

//	if (argc == 1)
//		_verbase = true;

#if LICENSE
	int lcs_chk = el_init("zonekey_mcu.lcs");
	if (lcs_chk == -1) {
		fprintf(stderr, "ERR: license check error\n");
		return -1;
	}
	else if (lcs_chk == -2) {
		fprintf(stderr, "ERR: license timeout\n");
		return -2;
	}

	const char *feathers = el_getfeatures();
	KVS fs = parse_feathers(feathers);

	// TODO: 检查是否限制同时启动的数目 ...
	KVS::const_iterator itf = fs.find("cap_max");
	if (itf != fs.end()) {
		_gc->cap_max = atoi(itf->second.c_str());
		if (_gc->cap_max == 0)
			_gc->cap_max = 1;
	}

#endif // 

	HANDLE env = OpenEvent(EVENT_MODIFY_STATE, 0, GLOBAL_NAME);
	if (env) {
		fprintf(stderr, "zonekey_mcu: only one instance running\n");
		CloseHandle(env);
		return -1;
	}
	else {
		env = CreateEvent(0, 0, 0, GLOBAL_NAME);
		_global_obj = env;

		if (!_verbase) {
			SERVICE_TABLE_ENTRY ServiceTable[] = {
				{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)mainp },
				{ 0, 0 },
			};

			// 启动服务的控制分派机线程，这个将直到服务结束后，返回
			StartServiceCtrlDispatcher(ServiceTable);
			return 0;
		}
		else {
			mainp(argc, argv);
			return 0;
		}
	}
#else // 
	// linux
	mainp(argc, argv);
	return 0;
#endif // 
}

#ifdef WIN32
static SERVICE_STATUS ServiceStatus;
static SERVICE_STATUS_HANDLE hStatus;
#define SVC_ERROR (0xe0000000)

VOID SvcReportEvent(LPTSTR szFunction) 
{ 
    HANDLE hEventSource;
    const char *lpszStrings[2];
    char Buffer[80];
  
    hEventSource = RegisterEventSource(NULL, SERVICE_NAME);
  
    if( NULL != hEventSource )
    {
        snprintf(Buffer, sizeof(Buffer), "%s failed with %d", szFunction, GetLastError());
  
        lpszStrings[0] = SERVICE_NAME;
        lpszStrings[1] = Buffer;
  
        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    SVC_ERROR,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data
  
        DeregisterEventSource(hEventSource);
    }
}

VOID ReportSvcStatus( DWORD dwCurrentState,
                        DWORD dwWin32ExitCode,
                        DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;
  
    // Fill in the SERVICE_STATUS structure.
    ServiceStatus.dwCurrentState = dwCurrentState;
    ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    ServiceStatus.dwWaitHint = dwWaitHint;
  
    if (dwCurrentState == SERVICE_START_PENDING)
        ServiceStatus.dwControlsAccepted = 0;
    else ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
  
    if ( (dwCurrentState == SERVICE_RUNNING) ||
            (dwCurrentState == SERVICE_STOPPED) )
        ServiceStatus.dwCheckPoint = 0;
    else ServiceStatus.dwCheckPoint = dwCheckPoint++;
  
    // Report the status of the service to the SCM.
    SetServiceStatus(hStatus, &ServiceStatus );
}

static void WINAPI ControlHandler(DWORD code)
{
	switch (code) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		_quit = 1;	// 结束服务
		ReportSvcStatus(ServiceStatus.dwCurrentState, NO_ERROR, 0);
		return;
	default:
		break;
	}
}

static int regWinService()
{
	hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, (LPHANDLER_FUNCTION)ControlHandler);
	if (!hStatus) {
		SvcReportEvent("RegisterServiceCtrlHandler");
		return -1;
	}

	char name[1024];
	GetModuleFileName(0, name, sizeof(name));

	char *p = strrchr(name, '\\');
	if (p) *p = 0;
	SetCurrentDirectory(name);

	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwServiceSpecificExitCode = 0;

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
	return 0;
}
#endif // 

extern "C" void libmsilbc_init(void);
#include "ver.h"

static void WINAPI mainp(int argc, char **argv)
{
#ifdef WIN32
	if (!_verbase) {
		if (regWinService() < 0)
			return;
	}
#endif // 

	log_init();
	ms_init();
	ortp_init();

#ifdef _DEBUG
	putenv("xmpp_domain=qddevsrv.zonekey");
#endif // debug	

	fprintf(stderr, "VER: %s\n", VERSION_STR);

	ortp_set_log_level_mask(ORTP_FATAL);
	zonekey_publisher_register();
	zonekey_void_register();
	zonekey_publisher_set_log_handler(log);
	libmsilbc_init();
	zonekey_audio_mixer_register();
	zonekey_audio_mixer_set_log_handler(log);
	zonekey_video_mixer_register();
	zonekey_video_mixer_set_log_handler(log);

	rtp_profile_set_payload(&av_profile, 100, &payload_type_h264);
	rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_wb);
	rtp_profile_set_payload(&av_profile, 102, &payload_type_ilbc);

	Server server;
	server.run(&_quit);

#ifdef WIN32
	if (!_verbase)
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
	CloseHandle(_global_obj);
#endif
}

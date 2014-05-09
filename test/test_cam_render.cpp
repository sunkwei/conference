#include <mediastreamer2/mediastream.h>
#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/msvideoout.h>

static void dump_webcam(MSWebCamDesc *desc)
{
	fprintf(stdout, "%s: device type='%s'\n", __FUNCTION__, desc->driver_type);
}

static MSFilter *get_source()
{
	MSWebCamManager *wcm = ms_web_cam_manager_get();
	const MSList *list = ms_web_cam_manager_get_list(wcm);

	const MSList *next = list->next;
	while (next) {
		MSWebCam *cam = (MSWebCam*)next->data;

		dump_webcam(cam->desc);

		next = next->next;
	}

	MSWebCam *cam = ms_web_cam_manager_get_default_cam(wcm);
	if (!cam) {
		fprintf(stderr, "ERR: NO cams!!!\n");
		return 0;
	}

	MSFilter *source = ms_web_cam_create_reader(cam);
	if (!source) {
		fprintf(stderr, "ERR: can't create Filter for %s\n", cam->desc->driver_type);
		ms_web_cam_destroy(cam);
		return 0;
	}

	return source;
}

static const char* choose_display_name()
{
#ifdef WIN32
	return ("MSDrawDibDisplay");
#elif defined(ANDROID)
	return ("MSAndroidDisplay");
#elif __APPLE__ && !defined(__ios)
	return ("MSOSXGLDisplay");
#elif defined(HAVE_GL)
	return ("MSGLXVideo");
#elif defined (HAVE_XV)
	return ("MSX11Video");
#elif defined(__ios)
	return ("IOSDisplay");	
#else
	return ("MSVideoOut");
#endif
}

static MSFilter *get_render()
{
	const char *name = choose_display_name();
	fprintf(stderr, "using render '%s'\n", name);
	MSFilter *render = ms_filter_new_from_name(name);
	return render;
}

int main()
{
	ms_init();

	MSFilter *source = get_source();
	MSFilter *render = get_render();

	ms_filter_link(source, 0, render, 0);

	MSTicker *ticker = ms_ticker_new();
	ms_ticker_attach(ticker, source);

	while (true) {
		ms_sleep(1);
	}

	ms_ticker_destroy(ticker);

	ms_filter_destroy(render);
	ms_filter_destroy(source);

	return 0;
}

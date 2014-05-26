#include <mediastreamer2/rfc3984.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/zk.video_recorder.h"

typedef struct zk_video_recrod_data
{
	ZonekeyRecordCallbackParam cb_data;
} zk_video_recrod_data;

static int _method_set_callback(MSFilter *f, void *args)
{
	zk_video_recrod_data *data = (zk_video_recrod_data *)(f->data);
	data->cb_data = *((ZonekeyRecordCallbackParam *)args);
	return 0;
}

static MSFilterMethod _methods [] = 
{
	{ ZONEKEY_METHOD_RECORD_SET_CALLBACK_PARAM, _method_set_callback },
	{ 0, 0 },
};



static void _init(struct _MSFilter *f)
{
	zk_video_recrod_data *data = (zk_video_recrod_data *) malloc(sizeof(zk_video_recrod_data));
	data->cb_data.on_data = NULL;
	f->data = data;
}

static void _process(struct _MSFilter *f)
{
	zk_video_recrod_data *data = (zk_video_recrod_data *)f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL) {
		if (data->cb_data.on_data) {
			uint8_t nalu_type = (*(m->b_rptr + 4)) & ((1<<5)-1);
			int is_key = (nalu_type == 7) || (nalu_type == 8) || (nalu_type ==5);
			data->cb_data.on_data(m->b_rptr, m->b_wptr-m->b_rptr, is_key, 
				mblk_get_timestamp_info(m), data->cb_data.userdata);
		}
		freemsg(m);
	}
}

static void _uninit(struct _MSFilter *f)
{
	zk_video_recrod_data *data = (zk_video_recrod_data *)f->data;
	free(data);
}

MSFilterDesc zkvideo_recorder_desc = 
{
	MS_FILTER_PLUGIN_ID,
	ZONEKEY_VIDEO_RECORDER_NAME,
	"zonekey video recorder",
	MS_FILTER_OTHER,
	0,
	1,	
	0,	
	_init,
	NULL,
	_process,
	NULL,
	_uninit,
	_methods,
	0
};

void zonekey_video_record_register()
{
	ms_filter_register(&zkvideo_recorder_desc);
}
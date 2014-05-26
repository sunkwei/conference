
#include "mediastreamer2/record_filter.h"


typedef struct zk_ms_record_data
{
	zkrecorder_cb_data cb_data;
} zk_ms_record_data;

static int _method_set_callback(MSFilter *f, void *args)
{
	zk_ms_record_data *data = (zk_ms_record_data *)(f->data);
	data->cb_data = *((zkrecorder_cb_data *)args);
	return 0;
}

static MSFilterMethod _methods [] = 
{
	{ ZKRECORDER_METHOD_SET_CALLBACK, _method_set_callback },
	{ 0, 0 },
};



static void _init(struct _MSFilter *f)
{
	zk_ms_record_data *data = ms_new(zk_ms_record_data, 1);
	f->data = data;
}

static void _process(struct _MSFilter *f)
{
	zk_ms_record_data *data = (zk_ms_record_data *)f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		mblk_t *it=m;
		ms_mutex_lock(&f->lock);

		while(it!=NULL){
		int len=it->b_wptr-it->b_rptr;
		if (len > 0)
		{
			data->cb_data.on_data(it->b_rptr, len, data->cb_data.userdata);
		}
			
		it=it->b_cont;
		}
		
		ms_mutex_unlock(&f->lock);
		freemsg(m);
	}
}

static void _uninit(struct _MSFilter *f)
{
	zk_ms_record_data *data = (zk_ms_record_data *)f->data;
	ms_free(data);
}

MSFilterDesc zkrecorder_desc = 
{
	MS_FILTER_PLUGIN_ID,
	"zkrecorder",
	"zonekey mediastream recorder",
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


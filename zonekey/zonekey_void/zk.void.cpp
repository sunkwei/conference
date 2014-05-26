#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.void.h>
#include <ortp/ortp.h>
#include <cc++/thread.h>
#include <deque>

namespace {
	class Source
	{
		ost::Mutex cs_;
		std::deque<mblk_t *> pending_;
		MSFilter *filter_;

	public:
		Source(MSFilter *f)
			: filter_(f)
		{
		}

		~Source()
		{
			while (!pending_.empty()) {
				mblk_t *m = pending_.front();
				pending_.pop_front();

				freemsg(m);
			}
		}

		int save(mblk_t *m)
		{
			ost::MutexLock al(cs_);
			pending_.push_back(m);
			return pending_.size();
		}

		int process()
		{
			// 从 pending_ 中取出，写入 output pin
			if (filter_->outputs[0] == 0) {
				// 没有连接
				return 0;
			}

			ost::MutexLock al(cs_);
			while (!pending_.empty()) {
				mblk_t *m = pending_.front();
				pending_.pop_front();

				save_m(m);
				ms_queue_put(filter_->outputs[0], m);
			}

			return 0;
		}

		void save_m(mblk_t *m)
		{
			// TODO:
		}
	};

	class Sink
	{
		MSFilter *filter_;
		ZonekeyVoidCallback cb_;

	public:
		Sink(MSFilter *f)
			: filter_(f)
		{
		}

		~Sink()
		{
		}

		void set_cb(ZonekeyVoidCallback *cb)
		{
			cb_.opaque = cb->opaque;
			cb_.on_data = cb->on_data;
		}

		int process()
		{
			while (mblk_t *m = ms_queue_get(filter_->inputs[0])) {
				save_m(m);
				if (cb_.on_data)
					cb_.on_data(cb_.opaque, m);
				freemsg(m);	// 不再使用了 XXX：此处释放，在回调中，如果希望修改 m，则必须 dupmsg(m)
			}

			return 0;
		}

		void save_m(mblk_t *m)
		{
//			FILE *fp = fopen("sink.pcm", "ab");
//			fwrite(m->b_rptr, 1, m->b_wptr - m->b_rptr, fp);
//			fclose(fp);
		}
	};
};

static void _source_init(MSFilter *f)
{
	f->data = new Source(f);
}

static void _source_process(MSFilter *f)
{
	((Source*)f->data)->process();
}

static void _source_uninit(MSFilter *f)
{
	delete (Source*)f->data;
}

static int _source_method_send(MSFilter *f, void *args)
{
	((Source*)f->data)->save((mblk_t*)args);
	return 0;
}

static MSFilterMethod _source_method[] = {
	{ ZONEKEY_METHOD_VOID_SOURCE_SEND, _source_method_send },
};

static MSFilterDesc _source_desc = {
	MS_FILTER_PLUGIN_ID,
	"ZonekeyVoidSource",
	"zonekey void source filter",
	MS_FILTER_OTHER,
	0,
	0,
	1,
	_source_init,
	0,
	_source_process,
	0,
	_source_uninit,
	_source_method,
	0,
};

static void _sink_init(MSFilter *f)
{
	f->data = new Sink(f);
}

static void _sink_process(MSFilter *f)
{
	((Sink*)f->data)->process();
}

static void _sink_uninit(MSFilter *f)
{
	delete ((Sink*)f->data);
}

static int _sink_method_set_callback(MSFilter *f, void *args)
{
	((Sink*)f->data)->set_cb((ZonekeyVoidCallback*)args);
	return 0;
}

static MSFilterMethod _sink_method[] = {
	{ ZONEKEY_METHOD_VOID_SINK_SET_CALLBACK, _sink_method_set_callback },
};

static MSFilterDesc _sink_desc = {
	MS_FILTER_PLUGIN_ID,
	"ZonekeyVoidSink",
	"zonekey void sink filter",
	MS_FILTER_OTHER,
	0,
	1, 
	0,
	_sink_init,
	0,
	_sink_process,
	0,
	_sink_uninit,
	_sink_method,
	0
};

void zonekey_void_register()
{
	ms_filter_register(&_source_desc);
	ms_filter_register(&_sink_desc);
}

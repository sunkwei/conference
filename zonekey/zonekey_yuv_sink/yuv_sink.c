/** 用于作为一个假的 video render，可以连接到 decoder 后，接受 yuv420p 的数据
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.yuv_sink.h>

typedef struct CTX
{
	ZonekeyYUVSinkCallbackParam callback_;
	ms_mutex_t cs_;

	int width_, height_;
} CTX;


static void _init(MSFilter *f)
{
	CTX *ctx = (CTX*)malloc(sizeof(CTX));
	ms_mutex_init(&ctx->cs_, 0);
	ctx->callback_.push = 0;
	ctx->width_ = ctx->height_ = 0;

	f->data = ctx;
}

static void _uninit(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ms_mutex_destroy(&ctx->cs_);

	free(ctx);
	f->data = 0;
}

static void _preprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
}

static void _postprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
}

static void _process(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;

	mblk_t *m = ms_queue_get(f->inputs[0]);	// 从 input 取出
	MSPicture pic;

	while (m) {
		// 此时收到完整 yuv 图像了
		int rc = ms_yuv_buf_init_from_mblk(&pic, m);
		ms_mutex_lock(&ctx->cs_);
		
		if (ctx->callback_.push) {
			ctx->callback_.push(ctx->callback_.ctx, pic.w, pic.h, pic.planes, pic.strides);
		}

		ms_mutex_unlock(&ctx->cs_);

		freemsg(m);

		m = ms_queue_get(f->inputs[0]);	// 继续
	}
}

static int _method_set_callback(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	ZonekeyYUVSinkCallbackParam *param = (ZonekeyYUVSinkCallbackParam*)args;

	// FIXME: 是否支持多路输出？？
	ms_mutex_lock(&ctx->cs_);
	ctx->callback_ = *param;
	ms_mutex_unlock(&ctx->cs_);

	return 0;
}

static int _method_set_width(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	ctx->width_ = *(int*)args;
	return 0;
}

static int _method_set_height(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	ctx->height_ = *(int*)args;
	return 0;
}

static MSFilterMethod _methods [] = 
{
	{ ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, _method_set_callback, },
	{ ZONEKEY_METHOD_YUV_SINK_SET_IMAGE_WIDTH, _method_set_width,},
	{ ZONEKEY_METHOD_YUV_SINK_SET_IMAGE_HEIGHT, _method_set_height,},
	{ 0, 0, },
};

static MSFilterDesc _desc = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyYUVSink",
	"zonekey YUV sink filter",
	MS_FILTER_OTHER,
	0,
	1,	// no input
	0,	// output
	_init,
	_preprocess,
	_process,
	_postprocess,
	_uninit,
	_methods,
	0,
};

void zonekey_yuv_sink_register()
{
	ms_filter_register(&_desc);
}

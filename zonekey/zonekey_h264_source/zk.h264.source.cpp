/** 一个 mediastreamer2 的 source filter，输出为经过 rfc3894 打包的 h264 流，允许直接链接 rtpsender filter

		提供一个 method 返回一个函数指针，用于写入 h264 帧数据
 */

#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/rfc3984.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/allfilters.h>
#include <mediastreamer2/zk.h264.source.h>
#include <deque>
#include <cc++/thread.h>

namespace {
typedef struct CTX
{			
	Rfc3984Context *rfc3984;	// 用于打包.
	std::deque<MSQueue *> fifo_;		// 在 writer() 中写入，在 _process() 中提取.
	ost::Mutex cs_fifo_;
} CTX;
};

static void _init(MSFilter *f)
{
	CTX *ctx = new CTX;
	f->data = ctx;

	ctx->rfc3984 = rfc3984_new();
	rfc3984_set_mode(ctx->rfc3984, 1);
}

static void _uninit(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	rfc3984_destroy(ctx->rfc3984);
	delete ctx;
}

static void _preprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
}

static void _postprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;

	// TODO: 这里释放掉所有没有被取出的.
}

static MSQueue *next_queue(CTX *ctx)
{
	MSQueue *queue = 0;
	ost::MutexLock al(ctx->cs_fifo_);
	if (!ctx->fifo_.empty()) {
		queue = ctx->fifo_.front();
		ctx->fifo_.pop_front();
	}
	return queue;
}

static void _process(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	MSQueue *queue = next_queue(ctx);

	while (queue) {
		/**  FIXME: 这里直接使用这个时间戳是不对的，应该使用 writer() 中的时间戳
					不过这个 _process 的调度频率相对视频帧率来说，比较高 (100fps）所以可能时间戳的偏移不会太严重 :)
		 */
		uint32_t ts=f->ticker->time*90LL;

		// 使用 rfc3984 打包，并且写到下一级 filter
		rfc3984_pack(ctx->rfc3984, queue, f->outputs[0], ts);
		ms_queue_destroy(queue);	// 不再使用.

		// 继续
		queue = next_queue(ctx);
	}
}

// 从 annex b 流中，查找下一个 startcode，可能是 00 00 00 01 或者 00 00 01
// from  libavformat/avc.c
static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
#if 0
	for (; p + 3 < end; p++) {
		if (p[0] == 0 && p[1] == 0) {
			if (p[2] == 1) return p;
			else if (p[2] == 0 && p[3] == 1) return p;
		}

		p++;
	}

	return end;
#else
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
#endif // 
}

/** 从 00 00 00 01 67 .... 的 h264 流，转换为 MSQueue
	保留 sps, pps ...
 */
static MSQueue *annexb2queue(const void *data, int len)
{
	MSQueue *queue = (MSQueue *)ms_new(MSQueue, 1);

	const unsigned char *head = (unsigned char*)data;
	const unsigned char *tail = head + len;
	const unsigned char *nal_start = ff_avc_find_startcode_internal(head, tail);
	const unsigned char *nal_end;
	mblk_t *nal;

	ms_queue_init(queue);

	for (;; ) {
		while (nal_start < tail && !*(nal_start++));
		if (nal_start == tail)
			break;

		nal_end = ff_avc_find_startcode_internal(nal_start, tail);

		// 此时 nal_start --> nal_end
		nal = allocb(nal_end - nal_start+10, 0);	// 多分配了10个字节.
		memcpy(nal->b_wptr, nal_start, nal_end - nal_start);
		nal->b_wptr += nal_end - nal_start;

		// 将 mblk 添加到 queue 中.
		ms_queue_put(queue, nal);
		
		// 下一个
		nal_start = nal_end;
	}

	return queue;
}

/** 应用程序外部调用，写入 h264 数据，放到缓冲中，
	在 _process() 时，再传递到下一个 filter
 */
static int _write_h264(void *c, const void *data, int len, double stamp)
{
	CTX *ctx = (CTX*)c;
	if (len > 0) {
		MSQueue *slices = annexb2queue(data, len);

		// 此时 slices 已经分解为 slice，缓存到 fifo 中，等待 process() 处理.
		ost::MutexLock al(ctx->cs_fifo_);
		ctx->fifo_.push_back(slices);
	}
	return 0;
}

static int _method_get_writer_param(MSFilter *f, void *args)
{
	ZonekeyH264SourceWriterParam *param = (ZonekeyH264SourceWriterParam*)args;
	param->ctx = f->data;
	param->write = _write_h264;
	return 0;
}

static MSFilterMethod _methods [] = 
{
	{ ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, _method_get_writer_param, },
	{ 0, 0, },
};

static MSFilterDesc _desc = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyH264Source",
	"zonekey h264 source filter",
	MS_FILTER_OTHER,
	0,
	0,	// no input
	1,	// output
	_init,
	_preprocess,
	_process,
	_postprocess,
	_uninit,
	_methods,
	0,
};

MS2_PUBLIC void zonekey_h264_source_register()
{
	ms_filter_register(&_desc);
	ms_message("zonekey h264 source filter registered!");
}

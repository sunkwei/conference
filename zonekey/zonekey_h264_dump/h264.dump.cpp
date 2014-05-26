#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <ortp/ortp.h>
#include <mediastreamer2/rfc3984.h>
#include <mediastreamer2/zk.h264.dump.h>

namespace
{
struct Ctx
{
	Rfc3984Context *rfc3984_;
	void (*push_)(void *opaque, const void *data, int len, double stamp, int key);
	void *opaque_;
	uint8_t *bitstream_;
	int bitstream_size_, bitstream_buf_;

	Ctx()
	{
		push_ = 0;
		rfc3984_ = rfc3984_new();
		rfc3984_set_mode(rfc3984_, 1);
		bitstream_ = (uint8_t*)malloc(128*1024);
	}

	~Ctx()
	{
		rfc3984_destroy(rfc3984_);
		free(bitstream_);
	}

	void process(MSFilter *f)
	{
		// 在 filter 工作线程中调用！
		// 从 input 获取 rtp 包，使用 rfc3984 解包，分发到 push()
		MSQueue nalus;
		ms_queue_init(&nalus);

		mblk_t *m = ms_queue_get(f->inputs[0]);
		while (m) {
			double stamp = mblk_get_timestamp_info(m);
			rfc3984_unpack(rfc3984_, m, &nalus);
			if (!ms_queue_empty(&nalus)) {
				// 完整的 slices，
				void *bits;
				int need_init, key;
				int size = nalus2frame(&nalus, &bits, &need_init, &key);
				if (size > 0 && push_) {
					push_(opaque_, bits, size, stamp, key);
				}
			}
			m = ms_queue_get(f->inputs[0]);
		}
	}

	// 从 nalus 组成 frame 字节流，annexb 格式
	int nalus2frame(MSQueue *nalus, void **bits, int *need_init, int *key)
	{
		mblk_t *m;
		uint8_t *dst = bitstream_;				// 缓冲
		uint8_t *end = dst + bitstream_buf_;	// 缓冲结束
		*need_init = 0;
		*key = 0;

		while (m = ms_queue_get(nalus)) {
			uint8_t *src = m->b_rptr;
			int nal_len = m->b_wptr - src;

			if (dst + nal_len + 32 > end) {
				// 需要扩展
				int pos = dst - bitstream_;
				int exp = (pos + nal_len + 32 + 4095) / 4096 * 4096;
				bitstream_ = (uint8_t*)realloc(bitstream_, exp);
				end = bitstream_ + exp;
				dst = pos + bitstream_;
			}

			if (src[0] == 0 && src[1] && src[2] == 0 && src[3] == 1) {
				// 呵呵，不应该出现这个情况，说明发送时，没有从 annexb 转换
				memcpy(dst, src, nal_len);
				dst += nal_len;
			}
			else {
				// 将 nal 数据复制到 dst，添加 00 00 00 01
				uint8_t type = (*src) & ((1 << 5) - 1);
				if (type == 7) {
					// TODO: sps，检查是否变化，如果变化，则设置 need_init
					*key = 1;
				}
				if (type == 8) {
					// TODO: pps，检查是否变化，如果变化，则设置 need_init
					*key = 1;
				}
				if (type == 5) {
					// IDR frame
					*key = 1;
				}

				// 总是增加 00 00 00 01
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 1;

				*dst++ = *src++;	// 复制 nal_type

				while (src < m->b_wptr - 3) {
					if (src[0] == 0 && src[1] == 0 && src[2] < 3) {
						// 需要特殊处理的码率字段
						*dst++ = 0;
						*dst++ = 0;
						*dst++ = 3;
						src += 2;
					}
					*dst++ = *src++;
				}
				// 循环中少了3个字节
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
			}

			freemsg(m);
		}

		return dst - bitstream_;
	}

	int set_callback(void (*push)(void *opaque, const void *data, int len, double stamp, int key), void *opaque)
	{
		push_ = push;
		opaque_ = opaque;
		return 0;
	}
};
};

static void _init(MSFilter *f)
{
	f->data = new Ctx;
}

static void _uninit(MSFilter *f)
{
	delete (Ctx*)f->data;
	f->data = 0;
}

static void _process(MSFilter *f)
{
	Ctx *ctx = (Ctx*)f->data;
	ctx->process(f);
}

static int _method_set_callback(MSFilter *f, void *args)
{
	Ctx *ctx = (Ctx*)f->data;
	ZonekeyH264DumpParam *param = (ZonekeyH264DumpParam*)args;
	return ctx->set_callback(param->push, param->opaque);
}

static MSFilterMethod _methods [] = 
{
	{ ZONEKEY_METHOD_H264_DUMP_SET_CALLBACK_PARAM, _method_set_callback, },
	{ 0, 0, },
};

static MSFilterDesc _desc = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyH264Dump",
	"zonekey h264 recver and dump",
	MS_FILTER_OTHER,
	0,
	1,
	0,
	_init,
	0,
	_process,
	0,
	_uninit,
	_methods,
	0,
};

void zonekey_h264_dump_register()
{
	ms_filter_register(&_desc);
}

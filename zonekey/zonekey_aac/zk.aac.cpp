/** 使用 libfaac 和 libfaad 实现 aac codec filter
 */
#include <stdio.h>
#include <stdlib.h>

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/zk.aac.h>
#include <faac.h>
#include <faad.h>

namespace {

#define DEF_RATE 32000
#define DEF_CH 1
#define DEF_BITS 16

class FaacEncoder
{
	int rate_, chs_, bits_;
	faacEncHandle enc_;
	unsigned long outbuf_size_, inbuf_samples_, once_size_;
	void *outbuf_, *inputbuf_;
	MSBufferizer inbuf_;
	uint32_t stamp_;

public:
	FaacEncoder()
	{
		rate_ = DEF_RATE;
		chs_ = DEF_CH;
		bits_ = DEF_BITS;
		enc_ = 0;
		inputbuf_ = 0;
		stamp_ = 0;

		qinit(&inbuf_.q);
		inbuf_.size = 0;
	}

	~FaacEncoder()
	{
		stop();
		flushq(&inbuf_.q, 0);
	}

	/// 使用参数启动，因此如果参数不同，必须 open 之前设置 ....
	int open()
	{
		enc_ = faacEncOpen(rate_, chs_, &inbuf_samples_, &outbuf_size_);
		if (enc_ == 0)
			return -1;

		faacEncConfiguration *cfg = faacEncGetCurrentConfiguration(enc_);
		cfg->aacObjectType = LOW;
		cfg->mpegVersion = MPEG4;
		cfg->inputFormat = FAAC_INPUT_16BIT;
		cfg->outputFormat = 1;	// with ADTS
		cfg->bitRate = 32000 / chs_;		// bitrate default == sample rate
		faacEncSetConfiguration(enc_, cfg);
		once_size_ = inbuf_samples_ * 2;		// 总是 16bits
		inputbuf_ = malloc(once_size_);

		return 0;
	}

	int stop()
	{
		if (enc_) {
			faacEncClose(enc_);
			enc_ = 0;
		}
		if (inputbuf_) {
			free(inputbuf_);
			inputbuf_ = 0;
		}

		return 0;
	}

	int run(MSFilter *f)
	{
		// 如果 f->input[0] 中的数据不足，需要等下一轮了 :)
		while (mblk_t *im = ms_queue_get(f->inputs[0])) {
			ms_bufferizer_put(&inbuf_, im);
		}

		while (ms_bufferizer_read(&inbuf_, (uint8_t*)inputbuf_, once_size_) == once_size_) {
			mblk_t *om = allocb(outbuf_size_, 0);
			int out = faacEncEncode(enc_, (int32_t*)inputbuf_, inbuf_samples_, (uint8_t*)om->b_wptr, outbuf_size_);
			if (out > 0) {
				om->b_wptr += out;
				stamp_ += inbuf_samples_ / chs_;
				mblk_set_timestamp_info(om, stamp_);
				ms_queue_put(f->outputs[0], om);
			}
			else
				freeb(om);
		}

		return 0;
	}
};
}

static void enc_init(MSFilter *f)
{
	f->data = new FaacEncoder;
}

static void enc_uninit(MSFilter *f)
{
	FaacEncoder *enc = (FaacEncoder*)f->data;
	delete enc;
}

static void enc_preprocess(MSFilter *f)
{
	FaacEncoder *enc = (FaacEncoder*)f->data;
	enc->open();
}

static void enc_postprocess(MSFilter *f)
{
	FaacEncoder *enc = (FaacEncoder*)f->data;
	enc->stop();
}

static void enc_process(MSFilter *f)
{
	FaacEncoder *enc = (FaacEncoder*)f->data;
	enc->run(f);
}

static MSFilterMethod enc_methods[] = 
{
	{ 0, 0, },
};

static MSFilterDesc _aac_encoder = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyAACEnc",
	"aac encoder using libfaac",
	MS_FILTER_ENCODER,
	"aac",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods,
};

void zonekey_aac_codec_register()
{
	ms_filter_register(&_aac_encoder);
}

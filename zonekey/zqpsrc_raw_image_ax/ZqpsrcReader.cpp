// ZqpsrcReader.cpp : CZqpsrcReader 的实现

#include "stdafx.h"
#include "ZqpsrcReader.h"
#include "ImageRGB24.h"
#include <assert.h>
// CZqpsrcReader



STDMETHODIMP CZqpsrcReader::Open(BSTR url)
{
	_bstr_t Url(url);

	zqpsrc_open(&src_, Url);
	if (src_) {
		AVCodec *codec = avcodec_find_decoder(CODEC_ID_H264);
		decoder_ = avcodec_alloc_context3(codec);
		assert(codec);
		avcodec_open(decoder_, codec);

		frame_ = avcodec_alloc_frame();
	}

	return S_OK;
}


STDMETHODIMP CZqpsrcReader::Close(void)
{
	if (src_) {
		zqpsrc_close(src_);
		src_ = 0;
	}

	if (decoder_) {
		avcodec_free_frame(&frame_);
		frame_ = 0;
		avcodec_close(decoder_);
		av_free(decoder_);
		decoder_ = 0;
	}

	return S_OK;
}


STDMETHODIMP CZqpsrcReader::GetNext(IDispatch** img)
{
	if (src_ == 0) *img = 0;
	while (true) {
		zq_pkt *pkt = zqpsrc_getpkt(src_);
		if (!pkt) {
			Close();
			*img = 0;
			return S_OK;
		}

		if (pkt->type == 1) {
			// video, decoder, swscale, ....
			AVPacket pkg;
			pkg.data = (uint8_t *)pkt->ptr;
			pkg.size = pkt->len;
			int got;

			if (avcodec_decode_video2(decoder_, frame_, &got, &pkg) > 0 && got) {
				// 解码成功，转换到 rgb24
				if (!sws_)
					sws_ = sws_getContext(decoder_->width, decoder_->height, PIX_FMT_YUV420P, decoder_->width, decoder_->height, PIX_FMT_BGR24, SWS_FAST_BILINEAR, 0, 0, 0);
				
				IZonekey_RawImage_RGB24 *image = 0;
				CImageRGB24::CreateInstance(&image);
				AVPicture &pic = ((CImageRGB24*)image)->prepare(PIX_FMT_RGB24, decoder_->width, decoder_->height);
				sws_scale(sws_, frame_->data, frame_->linesize, 0, decoder_->height, pic.data, pic.linesize);
				*img = image;
				return S_OK;
			}
		}

		zqpsrc_freepkt(src_, pkt);
	}

	return S_OK;
}

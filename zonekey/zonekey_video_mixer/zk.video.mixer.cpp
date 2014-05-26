#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.video.mixer.h>
#include <mediastreamer2/rfc3984.h>
#include <cc++/thread.h>
#include <assert.h>
#include <deque>
#include <vector>
#include <zonekey/zqsender.h>
extern "C" {
#	include <libswscale/swscale.h>
}

#include "Canvas.h"
#include "InputSource.h"
#include "X264.h"

static void (*_log)(const char *, ...) = 0;

// 仿照 audio mixer
#define MIXER_MAX_CHANNELS ZONEKEY_VIDEO_MIXER_MAX_CHANNELS

// 指向预览 output pin
#define PREVIEW_PIN MIXER_MAX_CHANNELS

// 指向 zqpkt 输出
#define ZQPKT_PIN PREVIEW_PIN+1

static double time_now()
{
	struct timeval tv;
	ost::gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/** TODO List:
		version 0.1: 
			1. process() 将数据缓存到 InputChannel：拉伸到指定大小；
			2. mixer thread 根据 zorder，从 InputChannel 中取出 yuv 数据，根据 InputChannel 的属性( x, y, alpha...）画到画布中
			   画完所有，通过 x264 压缩画布，使用 rfc3984 打包，保存到对应的 outputs 中；

		version 0.2: 未启动
			类似窗口管理，仅仅重画没有被覆盖的部分，这样将提高效率，不过，正常的视频会议一般情况下，不会有视频覆盖的情况的，即使有，
			往往也是小窗口的覆盖，对效率的实际影响，需要具体评估！！！

		version 0.3: 
			v0.1 中，在mixer thread 中直接写入 filter_->outputs 是不对的，因为存在两个工作线程同时访问 output queue 的竞争；
			在0.3中改为使用 fifo_，并且启动 MS_FILTER_IS_PUMP 选项

		version 0.4: zonekey.mixer.video 其实不需要针对每个 input 对应一个 output，应该输出两个 outputs，
			一个为 h264 一般链接 tee，一个为 yuv 一般链接 yuv sink
 */

#define VERSION "0.4.1"

namespace
{
	/** 矩形区域
	 */
	struct RECT
	{
		int x, y;
		int width, height;
	};

	/** 对应一个视频混合器，同时支持 MIXER_MAX_CHANNELS 输入和输出，每录输入源：
				input: [yuv420p, width, height, x, y, dx, dy, alpha] 每录输入包含 yuv 数据， 画到画布上的位置，透明度
				output: outputs[0]: [yuv420p] 混合后的 yuv 数据，用于预监窗口
				        outputs[1]: [h264 rtp profile] 输出为整个画布的 h264 压缩流，并且使用 rfc3984 打包，可以直接连接 rtp sender 发送

			mixer 自身应该有个 ticker：
			    每个 channel 写入后，保存到对应的缓存中，在 ticker 中，根据 Z-order，将缓存中的图像画到画布上，压缩，然后输出到每个 channel 对应的 output 上
	 */
	class CTX : ost::Thread
	{
		X264 *x264_;	// encoder
		InputSource *channels_[ZONEKEY_VIDEO_MIXER_MAX_CHANNELS+1];		// 所有可以使用的
		std::deque<MSQueue*> fifo_; // 用于保存 mixer thread 输出的数据，process() 从中取出
		ost::Mutex cs_fifo_;
		int zorder_[ZONEKEY_VIDEO_MIXER_MAX_CHANNELS+1];	// 记录 zorder，总是先画 zorder_[0], .....
														// 元素为 channal id
		Canvas *canvas_;		// 画布
		ost::Mutex cs_channels_;	// 控制 channels 变化
		ost::Event evt_req_, evt_res_; // 用于 reload() 
		int req_, res_;		// 用于描述请求信息
		bool quit_;	// 标记是否 attach 到 ticker

		ZonekeyVideoMixerZOrder t_channel_zorder_;	// 用于传递到工作线程
		ZonekeyVideoMixerEncoderSetting t_encoder_desc;	// 

		MSFilter *filter_;	// 用于mixer thread访问 filter 自身
		int want_mode_;		

		bool has_pending_;	// 是否还有需要提交到 output 的数据

		ZonekeyVideoMixerStats stat_;
		int enabled_;	// 是否启动

#ifdef _DEBUG
		void save_yuv_file(const char *prefix, MSPicture pic)
		{
			return;
			char filename[128];
			snprintf(filename, sizeof(filename), "%s%dx%d.yuv", prefix, pic.w, pic.h);
			FILE *fp = fopen(filename, "wb");
			if (fp) {
				unsigned char *y = pic.planes[0];
				for (int i = 0; i < pic.h; i++) {
					fwrite(y, 1, pic.w, fp);
					y += pic.strides[0];
				}

				y = pic.planes[1];
				for (int i = 0; i < pic.h/2; i++) {
					fwrite(y, 1, pic.w/2, fp);
					y += pic.strides[1];
				}

				y = pic.planes[2];
				for (int i = 0; i < pic.h/2; i++) {
					fwrite(y, 1, pic.w/2, fp);
					y += pic.strides[2];
				}

				fclose(fp);
			}
		}
#endif // 

		MSPicture avpic_mspic(AVPicture *pic, int w, int h)
		{
			MSPicture p;
			p.w = w, p.h = h;
			for (int i = 0; i < 3; i++) {
				p.planes[i] = pic->data[i];
				p.strides[i] = pic->linesize[i];
			}

			return p;
		}

		void run()
		{
			/** 工作线程，根据每次循环的实际消耗的时间，动态修正下一次等待的时间长度，尽量符合输出帧率

					FIXME: 可能使用几帧的平均值，更合理些！
			 */

			canvas_ = 0;
			x264_ = 0;

			int to_wait = 40;	// 缺省 25fps
			int delta = 0;		// 

			// 缺省参数，960x540，25fps, 800kbps
			X264Cfg x264_cfg;
			x264_cfg.width = 960;
			x264_cfg.height = 540;
			x264_cfg.fps = 1000.0 / to_wait;
			x264_cfg.kbitrate = 800;
			x264_cfg.gop = 50;

			x264_ = new X264(&x264_cfg);
			canvas_ = new Canvas(x264_cfg.width, x264_cfg.height);

			double last = time_now();
			double begin = last;

			bytes_ = 0;
			last_stamp_ = 0.0;
			first_stamp_ = 0.0;
			totle_bytes_ = 0;
			frames_ = 0;
			total_qp_ = qp_ = 0;

			while (!quit_) {
				if (has_req()) {
					// 有新的命令请求
					switch (req_) {
					case -1:
						// 此时要求结束
						res_ = -1;
						reply();
						quit_ = true;
						continue;
						break;

					case 1:
						// 此时设置了 Canvas Encoding 属性，一般需要修改 x264, Canvas ...
						res_ = reset(&t_encoder_desc);
						to_wait = (int)(1000.0 / t_encoder_desc.fps);	// 需要调整等待时间
						break;

					case 3:
						// 此时希望获取 canvas encoding 属性
						res_ = get_encoding_params(&t_encoder_desc);
						break;

					case 2:
						// 此时需要调整 zorder，这个操作其实放在主线程中也没关系
						res_ = rezorder(&t_channel_zorder_);
						break;
					}

					reply();
				}

				if (enabled_)
					one_step(last - begin, delta);	//
				else {
					//fprintf(stderr, "O");
				}

				double curr = time_now();
				int tick = (int)((curr - last) * 1000.0);	// 实际消耗时间
				if (tick > to_wait + 1) {
					delta++;	// 太耗时间了
				}
				else if (tick < to_wait - 1) {
					delta--;    // 调过了
				}

				last = curr;

				if (delta > to_wait) delta = to_wait;	// 安全检查
				ost::Thread::sleep(to_wait - delta);	// 实际等待的时间
			}

			delete x264_;
			x264_ = 0;

			delete canvas_;
			canvas_ = 0;
		}

		double first_stamp_;
		double last_stamp_;
		unsigned int bytes_;
		unsigned int totle_bytes_;
		unsigned int total_qp_, qp_;
		unsigned int frames_;

		// now 为启动工作线程后的秒数
		void one_step(double now, int odelta)
		{
			stat_.time = now;
			stat_.last_delta = odelta;

			unsigned int bytes = 0;
			int active = 0;

			if (!running_) return;

			/** 	从 InputSource 中取出 yuv 数据，根据 zorder_ 关系，计算出需要画的窗口，画到 Canvas 中
					所有画完后，从 Canvas 中取出画布，使用 x264 压缩，然后再将压缩后的数据放到每个 channels 的 output 上
			 */
			// 目前采用最简单的，直接根据 zorder_ 的顺序，依次画到画布中
			if (!canvas_ || !x264_) return;

			// step 0: 清空画布
			canvas_->clear();

			// step 1: 依次从 InputSource 中取出，并且画到画布中
			do {
				ost::MutexLock al(cs_channels_);
				for (int i = 0; zorder_[i] != -1; i++) {
					if (channels_[zorder_[i]]) {
						active++;
						InputSource *source = channels_[zorder_[i]];
						YUVPicture pic = source->get_pic();
						if (pic.width > 0 && pic.height > 0) {
							canvas_->draw_yuv(pic.pic.data, pic.pic.linesize, pic.x, pic.y, pic.width, pic.height, pic.alpha);
							avpicture_free(&pic.pic);	// 对应 get_pic() 中的 avpicture_alloc()
						}
					}
				}
			} while (0);

			stat_.active_input = active;

			// step 2: 从画布中取出，压缩
			AVPicture pic = canvas_->get_pic();
			X264FrameState state;
			MSQueue nals;
			mblk_t *m;
			ms_queue_init(&nals);

			x264_->encode(pic.data, pic.linesize, &nals, &state);
			bytes = state.bytes;

			stat_.sent_bytes += bytes;
			stat_.sent_frames ++;
			qp_ += state.qp;

#ifdef _DEBUG
			//static int __cnt = 0;
			//__cnt++;
			//if (__cnt % 100 == 0) {
			//	MSPicture mspic = avpic_mspic(&pic, canvas_->width(), canvas_->height());
			//	char filename_prefix[1024];
			//	snprintf(filename_prefix, sizeof(filename_prefix), "canvas_%d_", __cnt/100);
			//	fprintf(stderr, "%s.yuv saved\n", filename_prefix);
			//	save_yuv_file(filename_prefix, avpic_mspic(&pic, canvas_->width(), canvas_->height()));
			//}
#endif // 

			// step 3: 将 h264 nals 打包为 rtp profile
			MSQueue *rtps = ms_new(MSQueue, 1);	// rtps 保存到 fifo 中，在 process() 时取出，释放
			ms_queue_init(rtps);
			x264_->rfc3984_pack(&nals, rtps, (uint32_t)(now*90000.0));	// 将数据传递到 rtps queue 了，rfc3984打包，要求 90khz

			// step 4: 将 rtps 写入 fifo_ 中，将在 process() 中，从 fifo_ 中取出，写入 outputs[1]
			do {
				ost::MutexLock al(cs_fifo_);
				fifo_.push_back(rtps);
			} while (0);


			has_pending_ = true;

			bytes_ += bytes;
			totle_bytes_ += bytes;

			if (state.nals > 0) {
				frames_++;
				total_qp_ += state.qp;
			}

			if (now < first_stamp_ + 1.0) // 此时计算是不对的，等 1 秒后再说
				return;
			double delta = now - last_stamp_;
			if (delta > 5.0) {
				stat_.last_fps = frames_ / delta;
				stat_.last_kbps = bytes_ / delta;
				stat_.last_qp = qp_ * 1.0 / frames_;

				//fprintf(stderr, "[mixer thread]: delta=%d, qp=%d, lfps=%.2f, afps=%.3f, lbr=%.2fkbps: abr=%.3fkbps, aqp=%.2f\n", 
				//	odelta, state.qp, stat_.last_fps, stat_.avg_fps,
				//	bytes_/delta/125.0, totle_bytes_/(now - first_stamp_)/125.0, qp_*1.0/frames_);

				last_stamp_ = now;

				bytes_ = 0;
				frames_ = 0;
				qp_ = 0;
			}

			stat_.avg_fps = stat_.sent_frames / (now - first_stamp_);	// 总平均
			stat_.avg_kbps = stat_.sent_bytes / (now - first_stamp_);  // 总平均

			if (now < 5.0) {
				stat_.last_fps = stat_.avg_fps;
				stat_.last_kbps = stat_.avg_kbps;
			}
		}

		// 工作线程调用，检查是否有请求
		bool has_req()
		{
			bool rc = evt_req_.wait(0);
			if (rc)
				evt_req_.reset();
			return rc;
		}

		// 工作线程调用，当执行完 req 后，通知主线程
		void reply()
		{
			evt_req_.reset();
			evt_res_.signal();	// 通知执行完成
		}

		// 主线程调用，向工作线程发送消息，等待工作线程处理完出后，返回执行结果
		int req(int cmd)
		{
			// 更改设置，并且等待设置完成
			req_ = cmd;
			evt_req_.signal();
			evt_res_.wait();
			evt_res_.reset();

			return res_;
		}

		void swap(int *a, int *b)
		{
			int t = *a;
			*a = *b;
			*b = t;
		}

		// 调整 zorder，成功返回 0
		int rezorder(ZonekeyVideoMixerZOrder *desc)
		{
			ost::MutexLock al(cs_channels_);

			if (desc->id < 0 || desc->id >= ZONEKEY_VIDEO_MIXER_MAX_CHANNELS)	// 无效id
				return -1;

			if (desc->order_oper == ZONEKEY_VIDEO_MIXER_ZORDER_UP) {
				// 提前，实际是与后面的一个交换位置
				for (int i = 0; zorder_[i+1] != -1; i++) {
					if (zorder_[i] == desc->id) {
						swap(&zorder_[i], &zorder_[i+1]);
						break;
					}
				}
			}
			else if (desc->order_oper == ZONEKEY_VIDEO_MIXER_ZORDER_DOWN) {
				// 置后，实际是与前一个交换位置
				int i = 0;
				for (; zorder_[i] != -1; i++) {
					if (zorder_[i] == desc->id)
						break;
				}

				if (i > 0 && zorder_[i] != -1)
					swap(&zorder_[i-1], &zorder_[i]);
			}
			else if (desc->order_oper == ZONEKEY_VIDEO_MIXER_ZORDER_TOP) {
				// 置顶，放到队列的最后面
				for (int i = 0; zorder_[i+1] != -1; i++) {
					if (zorder_[i] == desc->id) {
						swap(&zorder_[i], &zorder_[i+1]);
					}
				}
			}
			else if (desc->order_oper == ZONEKEY_VIDEO_MIXER_ZORDER_BOTTOM) {
				// 置底，实际上是放到最前面
				int i = 0;
				for (; zorder_[i] != -1; i++) {
					if (zorder_[i] == desc->id)
						break;
				}

				if (zorder_[i] != -1) {
					// 存在
					for (int j = 0; j < i; j++)
						zorder_[j+1] = zorder_[j];

					zorder_[0] = desc->id;
				}
			}

			return 0;
		}

		// 重新设置 canvas 和 h264 属性，如果成功，返回 0，失败返回 -1
		int reset(ZonekeyVideoMixerEncoderSetting *setting)
		{
			// 是否需要重置 Canvas
			if (canvas_ && (canvas_->width() != setting->width || canvas_->height() != setting->height)) {
				delete canvas_;
				canvas_ = 0;
			}

			if (!canvas_) 
				canvas_ = new Canvas(setting->width, setting->height);

			if (x264_) {
				delete x264_;
				x264_ = 0;
			}

			X264Cfg cfg;
			cfg.width = setting->width;
			cfg.height = setting->height;
			cfg.kbitrate = setting->kbps;
			cfg.fps = setting->fps;
			cfg.gop = setting->gop;

			x264_ = new X264(&cfg);

			return 0;
		}

		int get_encoding_params(ZonekeyVideoMixerEncoderSetting *setting)
		{
			if (canvas_) {
				setting->width = canvas_->width();
				setting->height = canvas_->height();
			}
			else {
				setting->width = setting->height = 0;
			}

			if (x264_) {
				setting->fps = x264_->cfg()->fps;
				setting->kbps = x264_->cfg()->kbitrate;
				setting->gop = x264_->cfg()->gop;
			}

			return 0;
		}

	public:
		bool running_;

		CTX(MSFilter *f)
			: filter_(f)
		{
			for (int i = 0; i <= ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
				zorder_[i] = -1;	// 无效序号填充
				channels_[i] = new InputSource;
			}

			has_pending_ = false;
			running_ = false;
			enabled_ = -1;

			memset(&stat_, 0, sizeof(stat_));

			// 启动工作线程
			quit_ = false;
			start();
		}

		~CTX()
		{
			req(-1);	// 要求结束
			join();

			for (int i = 0; i <= ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
				delete channels_[i];
			}
		}

		int get_channel()
		{
			for (int i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
				if (channels_[i]->idle()) {
					channels_[i]->employ();

					ost::MutexLock al(cs_channels_);

					// 新建窗口总是放在 zorder 的最上面
					int z;
					for (z = 0; zorder_[z] != -1; z++){}
					zorder_[z] = i;

					return i;
				}
			}
			return -1;	// 找不到空闲！！！！
		}

		// 释放指定的 channel
		int free_channel(int cid)
		{
			if (cid >= 0 && cid < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS) {
				ost::MutexLock al(cs_channels_);
	
				channels_[cid]->disemploy();	// 释放，空闲

				// 从 zorder_ 中删除
				int i;
				for (i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
					if (zorder_[i] == cid)
						break;
				}

				// 此时 i 指向需要删除的节点，直接使用后面的覆盖前面的即可
				for (; zorder_[i] != -1; i++) {
					zorder_[i] = zorder_[i+1];
				}
				zorder_[i] = -1;

				return 0;
			}
			else
				return -1;
		}

		// 设置 channel 属性，这个可能是动态执行的，如窗口大小变化之类的
		int set_channel_desc(ZonekeyVideoMixerChannelDesc *desc)
		{
			// 直接设置到对应的 InputSource 上即可
			ost::MutexLock al(cs_channels_);

			if (desc->id < 0 || desc->id >= ZONEKEY_VIDEO_MIXER_MAX_CHANNELS)
				return -1;

			if (channels_[desc->id]->idle())
				return -1;

			channels_[desc->id]->set_param(desc->x, desc->y, desc->width, desc->height, desc->alpha);
			return 0;
		}

		int get_channel_desc(ZonekeyVideoMixerChannelDesc *desc)
		{
			ost::MutexLock al(cs_channels_);

			if (desc->id < 0 || desc->id >= ZONEKEY_VIDEO_MIXER_MAX_CHANNELS)
				return -1;

			if (channels_[desc->id]->idle()) {
				desc->x = desc->y = desc->width = desc->height = desc->alpha = 0;
			}
			else {
				InputSource *s = channels_[desc->id];
				// FIXME: 如果刚刚经过 set channel desc，则获取 x()... 会出错，不如直接获取 wanted_x
				//desc->x = s->x();
				//desc->y = s->y();
				//desc->width = s->width();
				//desc->height = s->height();
				//desc->alpha = s->alpha();

				desc->x = s->want_x();
				desc->y = s->want_y();
				desc->width = s->want_width();
				desc->height = s->want_height();
				desc->alpha = s->want_alpha();
			}

			return 0;
		}

		int get_zorder_array(ZonekeyVideoMixerZorderArray *arr)
		{
			ost::MutexLock al(cs_channels_);
			for (int i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS + 1; i++)
				arr->orders[i] = zorder_[i];

			return 0;
		}

		// 修改 channel 的 z-order
		int set_zorder(ZonekeyVideoMixerZOrder *desc)
		{
			t_channel_zorder_ = *desc;
			return req(2);	// 将在工作线程中执行 rezorder()
		}

		// 设置 x264 编码属性，支持中间修改 :)
		int set_encoder(ZonekeyVideoMixerEncoderSetting *setting)
		{
			t_encoder_desc = *setting;
			return req(1);	// 将在工作线程中执行 reset()
		}

		int get_encoder(ZonekeyVideoMixerEncoderSetting *setting)
		{
			req(3);
			*setting = t_encoder_desc;
			return 0;
		}

		void encode_enable(int en)
		{
			enabled_ = en;
		}

		// 返回统计信息
		int get_stats(ZonekeyVideoMixerStats *stat)
		{
			*stat = stat_;
			return 0;
		}

		void preprocess()
		{
			// 清空 fifo_
			ost::MutexLock al(cs_fifo_);
			while (!fifo_.empty()) {
				MSQueue *q = fifo_.front();
				ms_queue_flush(q);
				ms_free(q);
				fifo_.pop_front();
			}

			x264_->force_key_frame();	// 强制关键帧
			running_ = true;
		}

		void postprocess()
		{
			running_ = false;
		}

		// 处理 process()
		void channel_process(MSFilter *f)
		{
			/** 此调用来自每个 rtp recv 的线程！

				step 1:
					根据 zorder，计算出自身需要重画的部分，一般为一个矩形列表
					将需要重画的部分直接画到画布上。

				step 2:
					从 fifos_ 中取出 queue，并且写入 outputs

			 */
			for (int i = 0; i < ZONEKEY_VIDEO_MIXER_MAX_CHANNELS; i++) {
				if (f->inputs[i]) {
					mblk_t *m = ms_queue_get(f->inputs[i]);
					while (m) {
						if (enabled_) {
							MSPicture pic;
							ms_yuv_buf_init_from_mblk(&pic, m);
							channels_[i]->save_pic(&pic);	// 缓存
						}
						freemsg(m);	// 不再使用！
						m = ms_queue_get(f->inputs[i]);
					}
				}
			}

			if (!has_pending_) return;

			// 检查 fifo_，如果有数据，取出并扔到 outputs[1] 中
			do {
				ost::MutexLock al(cs_fifo_);
				while (!fifo_.empty()) {
					MSQueue *q = fifo_.front();
					if (f->outputs[1]) {
						mblk_t *m = ms_queue_get(q);
						while (m) {
							ms_queue_put(f->outputs[1], m);	// 写入 output[1] 
							m = ms_queue_get(q);
						}
					}
					ms_free(q);	// 对应着 rtps = ms_new()
					fifo_.pop_front();
				}
			} while (0);

			has_pending_ = false;
		}
	};
};

static void _init(MSFilter *f)
{
	CTX *ctx = new CTX(f);
	f->data = ctx;
}

static void _uninit(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	delete ctx;
	f->data = 0;
}

static void _preprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->preprocess();
}

static void _postprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->postprocess();
}

static void _process(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->channel_process(f);
}

static int _method_get_channel(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	int cid = ctx->get_channel();
	if (cid >= 0) {
		*(int*)args = cid;
		return 0;
	}
	return -1;
}

static int _method_free_channel(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	if (ctx->free_channel(*(int*)args) < 0)
		return -1;
	else
		return 0;
}

static int _method_set_channel_desc(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->set_channel_desc((ZonekeyVideoMixerChannelDesc*)args);
}

static int _method_set_channel_zorder(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->set_zorder((ZonekeyVideoMixerZOrder*)args);
}

static int _method_set_encoder(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->set_encoder((ZonekeyVideoMixerEncoderSetting*)args);
}

static int _method_get_encoder(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->get_encoder((ZonekeyVideoMixerEncoderSetting*)args);
}

static int _method_get_stats(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->get_stats((ZonekeyVideoMixerStats*)args);
}

static int _method_get_zorder_array(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->get_zorder_array((ZonekeyVideoMixerZorderArray*)args);
}

static int _method_get_channel_desc(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	return ctx->get_channel_desc((ZonekeyVideoMixerChannelDesc*)args);
}

static int _method_enable(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	ctx->encode_enable((int)args);
	return 0;
}

static MSFilterMethod _methods[] = 
{
	{ ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL, _method_get_channel },
	{ ZONEKEY_METHOD_VIDEO_MIXER_FREE_CHANNEL, _method_free_channel },
	{ ZONEKEY_METHOD_VIDEO_MIXER_SET_CHANNEL_DESC, _method_set_channel_desc },
	{ ZONEKEY_METHOD_VIDEO_MIXER_SET_ZORDER, _method_set_channel_zorder },
	{ ZONEKEY_METHOD_VIDEO_MIXER_SET_ENCODER_SETTING, _method_set_encoder },
	{ ZONEKEY_METHOD_VIDEO_MIXER_GET_ENCODER_SETTING, _method_get_encoder },
	{ ZONEKEY_METHOD_VIDEO_MIXER_GET_STATS, _method_get_stats },
	{ ZONEKEY_METHOD_VIDEO_MIXER_GET_ZOEDER, _method_get_zorder_array },
	{ ZONEKEY_METHOD_VIDEO_MIXER_GET_CHANNEL_DESC, _method_get_channel_desc },
	{ ZONEKEY_METHOD_VIDEO_MIXER_ENABLE, _method_enable },
	{ 0, 0, },
};

static MSFilterDesc _desc = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyVideoMixer",
	"zonekey video mixer filter",
	MS_FILTER_OTHER,
	0,
	MIXER_MAX_CHANNELS,		// inputs
	2,	// outputs: 0: yuv 预监输出，1: h264 码流输出
	_init,
	_preprocess,
	_process,
	_postprocess,
	_uninit,
	_methods,
	MS_FILTER_IS_PUMP,   // 因为 _process() 不直接将 input 的数据处理到 output 中，而是检查 fifos_ 中是否有数据
};

void zonekey_video_mixer_register()
{
	ms_filter_register(&_desc);
}

void zonekey_video_mixer_set_log_handler(void (*func)(const char *, ...))
{
	_log = func;
}

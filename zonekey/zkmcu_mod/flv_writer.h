/** zonekey flv writer，提供一个生成 flv 文件的接口
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif // c++

	typedef struct flv_writer_t flv_writer_t;

	/** 打开一个 flv writer 
	 */
	flv_writer_t *flv_writer_open(const char *filename);

	/** 结束 */
	void flv_writer_close(flv_writer_t *flv);

	/** 写入 h264 视频数据
	 */
	int flv_writer_save_h264(flv_writer_t *flv, const void *data, int len, 
		int key, double stamp,
		int width, int height);

	/** 写入 aac 音频数据
	 */
	int flv_writer_save_aac(flv_writer_t *flv, const void *data, int len, double stamp);

	/** 写入 speex 音频数据
	 */
	int flv_writer_save_speex(flv_writer_t *flv, const void *data, int len,
		double stamp,
		int channels, int rate, int bits);


#ifdef __cplusplus
}
#endif // c++

#ifndef _video_render_inf__
#define _video_render_inf__

#ifdef __cplusplus
extern "C" {
#endif // c++

typedef void *VideoCtx;
#include <libswscale/swscale.h>

/// the source data format
#if 1
typedef PixelFormat RVPixFmt;
#else
enum RVPixFmt
{
    RV_PIX_FMT_NONE= -1,
    RV_PIX_FMT_YUV420P,   ///< Planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
    RV_PIX_FMT_YUYV422,   ///< Packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
    RV_PIX_FMT_RGB24,     ///< Packed RGB 8:8:8, 24bpp, RGBRGB...
    RV_PIX_FMT_BGR24,     ///< Packed RGB 8:8:8, 24bpp, BGRBGR...
    RV_PIX_FMT_YUV422P,   ///< Planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
    RV_PIX_FMT_YUV444P,   ///< Planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
    RV_PIX_FMT_RGB32,     ///< Packed RGB 8:8:8, 32bpp, (msb)8A 8R 8G 8B(lsb), in cpu endianness
    RV_PIX_FMT_YUV410P,   ///< Planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
    RV_PIX_FMT_YUV411P,   ///< Planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
    RV_PIX_FMT_RGB565,    ///< Packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), in cpu endianness
    RV_PIX_FMT_RGB555,    ///< Packed RGB 5:5:5, 16bpp, (msb)1A 5R 5G 5B(lsb), in cpu endianness most significant bit to 0
    RV_PIX_FMT_GRAY8,     ///<        Y        ,  8bpp
    RV_PIX_FMT_MONOWHITE, ///<        Y        ,  1bpp, 0 is white, 1 is black
    RV_PIX_FMT_MONOBLACK, ///<        Y        ,  1bpp, 0 is black, 1 is white
    RV_PIX_FMT_PAL8,      ///< 8 bit with PIX_FMT_RGB32 palette
    RV_PIX_FMT_YUVJ420P,  ///< Planar YUV 4:2:0, 12bpp, full scale (jpeg)
    RV_PIX_FMT_YUVJ422P,  ///< Planar YUV 4:2:2, 16bpp, full scale (jpeg)
    RV_PIX_FMT_YUVJ444P,  ///< Planar YUV 4:4:4, 24bpp, full scale (jpeg)
    RV_PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing(xvmc_render.h)
    RV_PIX_FMT_XVMC_MPEG2_IDCT,
    RV_PIX_FMT_UYVY422,   ///< Packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
    RV_PIX_FMT_UYYVYY411, ///< Packed YUV 4:1:1, 12bpp, Cb Y0 Y1 Cr Y2 Y3
    RV_PIX_FMT_BGR32,     ///< Packed RGB 8:8:8, 32bpp, (msb)8A 8B 8G 8R(lsb), in cpu endianness
    RV_PIX_FMT_BGR565,    ///< Packed RGB 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), in cpu endianness
    RV_PIX_FMT_BGR555,    ///< Packed RGB 5:5:5, 16bpp, (msb)1A 5B 5G 5R(lsb), in cpu endianness most significant bit to 1
    RV_PIX_FMT_BGR8,      ///< Packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
    RV_PIX_FMT_BGR4,      ///< Packed RGB 1:2:1,  4bpp, (msb)1B 2G 1R(lsb)
    RV_PIX_FMT_BGR4_BYTE, ///< Packed RGB 1:2:1,  8bpp, (msb)1B 2G 1R(lsb)
    RV_PIX_FMT_RGB8,      ///< Packed RGB 3:3:2,  8bpp, (msb)2R 3G 3B(lsb)
    RV_PIX_FMT_RGB4,      ///< Packed RGB 1:2:1,  4bpp, (msb)1R 2G 1B(lsb)
    RV_PIX_FMT_RGB4_BYTE, ///< Packed RGB 1:2:1,  8bpp, (msb)1R 2G 1B(lsb)
    RV_PIX_FMT_NV12,      ///< Planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 for UV
    RV_PIX_FMT_NV21,      ///< as above, but U and V bytes are swapped

    RV_PIX_FMT_RGB32_1,   ///< Packed RGB 8:8:8, 32bpp, (msb)8R 8G 8B 8A(lsb), in cpu endianness
    RV_PIX_FMT_BGR32_1,   ///< Packed RGB 8:8:8, 32bpp, (msb)8B 8G 8R 8A(lsb), in cpu endianness

    RV_PIX_FMT_GRAY16BE,  ///<        Y        , 16bpp, big-endian
    RV_PIX_FMT_GRAY16LE,  ///<        Y        , 16bpp, little-endian
    RV_PIX_FMT_YUV440P,   ///< Planar YUV 4:4:0 (1 Cr & Cb sample per 1x2 Y samples)
    RV_PIX_FMT_YUVJ440P,  ///< Planar YUV 4:4:0 full scale (jpeg)
    RV_PIX_FMT_YUVA420P,  ///< Planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)
    RV_PIX_FMT_NB,        ///< number of pixel formats, DO NOT USE THIS if you want to link with shared libav* because the number of formats might differ between versions
};
#endif // 0

#ifdef X11
#include <X11/Xlib.h>
struct RVx11Wnd
{
	Display *display;	// from a success XOpenDisplay()
	Drawable d;			// from a success XCreateWindow()/XCreateSimpleWindow()
	GC gc;				// from a success XCreateGC()
};
#endif // x11

/** open a video render instance
  @param ctx: OUT param
	@param hwnd: the target window handler
					if WIN32, hwnd is the target window handler
					if X11, hwnd is a point to RVx11Wnd struct
	@param width: picture width
	@param height: picture height
	@return <0 ERR
*/
int rv_open (VideoCtx *ctx, void *hwnd, int width, int height);
int rv_open2 (VideoCtx *ctx, void *hwnd, int width, int height, int scalemode);
void rv_close (VideoCtx ctx);

/** 设置获取窗口位置的函数
*/
typedef void (*PFN_getSite)(void *ins, int *left, int *right, int *top, int *bottom);
void rv_set_cb_getsite (VideoCtx ctx, void *ins, PFN_getSite proc);


/** write pic data to target window
  	@param ctx: the render
	@param pixfmt: source data format
	@param data: source data
	@param stride: source data
	@return <0 ERR
  */
int rv_writepic (VideoCtx ctx, RVPixFmt pixfmt, 
		unsigned char *data[4], int stride[4]);

/** to redraw the last picture
 */
int rv_redraw (VideoCtx ctx);

#ifdef __cplusplus
}
#endif // c++

#endif // video-inf.h


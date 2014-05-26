//#include "ZKDShow.h"

#include <Windows.h>


#ifdef __cplusplus
extern "C" {
#endif // c++

	typedef struct zkDshow zkDshow;

	zkDshow* InitDshowPlayer(HWND disPlayWnd);


	void UnInitDshowPlayer(zkDshow *pDshow);


	void ZKPlay(zkDshow *pDshow);


	void ZKStop(zkDshow *pDshow);


	void PutData(zkDshow *pDshow, const void* pData, unsigned int pts, unsigned int nVideoWidth=0, unsigned int nVideoHeight=0);


	void ResizeWindow(zkDshow *pDshow, long inLeft, long inTop, long inWidth, long inHeight);


#ifdef __cplusplus
}
#endif // c++
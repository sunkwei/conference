
#ifdef __cplusplus
extern "C" {
#endif // c++

	typedef struct zkDshow zkDshow;

	zkDshow* InitDshowPlayer(HWND disPlayWnd);


	void UnInitDshowPlayer(zkDshow *pDshow);


	void ZKPlay(zkDshow *pDshow);


	void ZKStop(zkDshow *pDshow);


	void PutData(zkDshow *pDshow, const void* pData, unsigned int len, unsigned int pts);


	void ResizeWindow(zkDshow *pDshow, long inLeft, long inTop, long inWidth, long inHeight);

	unsigned long GetDelay(zkDshow *pDshow);
	void SetDelay(zkDshow *pDshow, unsigned long newVal);


#ifdef __cplusplus
}
#endif // c++

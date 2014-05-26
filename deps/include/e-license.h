#ifndef _e_license_hh
#define _e_license_hh

#ifdef __cplusplus
extern "C" {
#endif // c++

/** init epower license lib, MUST be called before any other el_xxx() functions, except 
  		el_makelcsfile()

	@param lcsfile: the file name of license.
  	@return >0 OK，else err code
		-1 invalid license file
		-2 timeout
 */
int el_init (const char *lcsfile);

int el_init2 (const char *lcsfile, const char *hwid);

/** to return NTP server time
 */
time_t el_ntp_time();

/** get hwid
 */
const char *el_gethwid ();

/** get feature list
 */
const char *el_getfeatures ();

/** 根据 raw file，生成 license 文件
 */
int el_makelcsfile (const char *rawfile, const char *lcsfile);

#ifdef __cplusplus
}
#endif // c++

#endif // e-license.h


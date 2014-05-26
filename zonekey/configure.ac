AC_INIT([zonekeymcu],[0.0.1])

AC_CONFIG_SRCDIR([zonekey_mcu/main.cpp])
AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

CFLAGS="$CFLAGS -Wall"
CXXFLAGS="$CXXFLAGS -Wall"
LDFLAGS="$LDFLAGS -lortp -lmediastreamer_voip -lmediastreamer_base -lccgnu2"

AC_OUTPUT(
	Makefile
	zonekey_publisher/Makefile
	zonekey_audio_mixer/Makefile
	zonekey_video_mixer/Makefile
	zonekey_mcu/Makefile
	)


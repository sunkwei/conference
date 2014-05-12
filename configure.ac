#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(conference, 0.1, sunkwei@gmail.com)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR(mcu/mcu/main.cpp)
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB(ortp, ortp_init, [],
				[
					echo 'Error: not found oRTP lib.'
					exit -1
				])
AC_CHECK_LIB(mediastreamer_voip, ms_voip_init, [],
				[
					echo 'Error: not found mediastreamer lib.'
					exit -1
				])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h strophe.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday strchr])

AC_OUTPUT(Makefile
		  mcu/Makefile
		  mcu/mcu/Makefile
		  test/Makefile
		 )
	

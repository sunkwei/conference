AM_CPPFLAGS=-I/usr/include/ffmpeg -D__STDC_CONSTANT_MACROS
lib_LTLIBRARIES= libzonekey_video_mixer.la
libzonekey_video_mixer_la_SOURCES= Canvas.cpp \
				   InputSource.cpp \
				   X264.cpp \
				   zk.video.mixer.cpp \
				   ZOrderMgrt.cpp
libzonekey_video_mixer_la_LDFLAGS= -lx264 -lpthread

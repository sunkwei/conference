AM_CFLAGS= -I/usr/local/include
EXT_LIBS= /usr/local/lib/libzkrobot_ex.a /usr/local/lib/libstrophe.a

bin_PROGRAMS=audio_conference_service
audio_conference_service_SOURCES= Conference.cpp main.cpp server.cpp
audio_conference_service_LDADD= ${EXT_LIBS} -lmediastreamer_voip -lmediastreamer_base -lortp -lccext2 -lccgnu2 -lexpat -lssl

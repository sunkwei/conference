bin_PROGRAMS= zonekey_mcu
zonekey_mcu_SOURCES= 	Conference.cpp \
			FreeConference.cpp \
			DirectorConference.cpp \
			H264Sink.cpp \
			H264Source.cpp \
			log.cpp \
			main.cpp \
			Server.cpp \
			Sink.cpp \
			Source.cpp \
			SpeexSink.cpp \
			SpeexSource.cpp \
			Stream.cpp \
			H264Stream.cpp \
			SpeexStream.cpp \
			util.cpp
zonekey_mcu_LDFLAGS=	-lortp -lmediastreamer_voip -lmediastreamer_base \
			../zonekey_publisher/libzonekey_publisher.la \
			../zonekey_audio_mixer/libzonekey_audio_mixer.la \
			../zonekey_video_mixer/libzonekey_video_mixer.la \
			/usr/local/lib/libzkrobot_ex.a \
			/usr/local/lib/libstrophe.a \
			-lexpat -lssl -lresolv

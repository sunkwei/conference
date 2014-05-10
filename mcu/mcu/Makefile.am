INCLUDES=	-I /usr/local/include

bin_PROGRAMS=	mcu
mcu_SOURCES=	main.cpp \
				Conference.cpp \
				Member.cpp \
				KVConfig.cpp \
				log.cpp \
				Server.cpp \
				Sink.cpp \
				Source.cpp \
				Stream.cpp \
				util.cpp \
				webapi.cpp \
				WsImpl.cpp \
				../gsoap/soapC.cpp \
				../gsoap/soapServer.cpp


mcu_LDADD=	-lccgnu2 -lgsoap++ -lmediastreamer_base -lmediastreamer_voip -lortp -lpthread -lrt

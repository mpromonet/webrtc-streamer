CC = $(CROSS)g++
AR = $(CROSS)ar
CFLAGS = -W -pthread -g -std=gnu++0x
TARGET = webrtc-server

# mongoose
CFLAGS += -I mongoose

# live555
CFLAGS += -I /usr/include/liveMedia  -I /usr/include/groupsock -I /usr/include/UsageEnvironment -I /usr/include/BasicUsageEnvironment/
LDFLAGS += -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment

# webrtc
WEBRTCROOT=../webrtc
WEBRTCBUILD=Release
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(GYP_GENERATOR_OUTPUT)/out/$(WEBRTCBUILD)

CFLAGS += -DWEBRTC_POSIX -fno-rtti
CFLAGS += -I $(WEBRTCROOT)/src -I $(WEBRTCROOT)/src/chromium/src/third_party/jsoncpp/source/include
LDFLAGS += -lX11 -lXext -lexpat -ldl -lnss3 -lnssutil3 -lplc4 -lnspr4 

all: $(TARGET)

libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a:
	$(AR) -rcT $@ $(shell find $(WEBRTCLIBPATH) -name '*.a')

mongoose/mongoose.c: 
	git submodule init
	git submodule update
	
$(TARGET): main.cpp PeerConnectionManager.cpp mongoose/mongoose.c libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)	

clean:
	rm -f *.o *.a $(TARGET)

CC = $(CROSS)g++ $(foreach sysroot,$(SYSROOT),--sysroot=$(sysroot))
AR = $(CROSS)ar
CFLAGS = -W -pthread -g -std=gnu++0x

# live555
ifneq ($(wildcard /usr/include/liveMedia/liveMedia.hh),)
	CFLAGS += -DHAVE_LIVE555
	CFLAGS += -I /usr/include/liveMedia  -I /usr/include/groupsock -I /usr/include/UsageEnvironment -I /usr/include/BasicUsageEnvironment/
	LDFLAGS += -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment
endif

# webrtc
WEBRTCROOT=../webrtc
WEBRTCBUILD=Release
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(GYP_GENERATOR_OUTPUT)/out/$(WEBRTCBUILD)

CFLAGS += -DWEBRTC_POSIX -fno-rtti
CFLAGS += -I $(WEBRTCROOT)/src -I $(WEBRTCROOT)/src/chromium/src/third_party/jsoncpp/source/include
LDFLAGS += -lX11 -ldl

TARGET = webrtc-server_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD)
all: $(TARGET)

WEBRTC_LIB = $(shell find $(WEBRTCLIBPATH) -name '*.a')
libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a: $(WEBRTC_LIB)
	$(AR) -rcT $@ $^

$(TARGET): main.cpp PeerConnectionManager.cpp libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)	

clean:
	rm -f *.o libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a $(TARGET)

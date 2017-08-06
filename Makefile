CC = $(CROSS)gcc 
CXX = $(CROSS)g++
AR = $(CROSS)ar
SYSROOTOPT=$(foreach sysroot,$(SYSROOT),--sysroot=$(sysroot))
CFLAGS = -Wall -pthread -g -std=c++11 -Iinc $(SYSROOTOPT) $(CFLAGS_EXTRA)
LDFLAGS = -pthread $(SYSROOTOPT)
WEBRTCROOT?=../webrtc
WEBRTCBUILD?=Release
PREFIX?=/usr
GITVERSION=$(shell git describe --tags --always --dirty)
VERSION=$(GITVERSION)

TARGET = $(notdir $(CURDIR))
all: $(TARGET)

# webrtc
VERSION+=webrtc@$(shell git -C $(WEBRTCROOT)/src describe --tags --always --dirty)
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(GYP_GENERATOR_OUTPUT)/out/$(WEBRTCBUILD)

CFLAGS += -DWEBRTC_POSIX -fno-rtti -D_GLIBCXX_USE_CXX11_ABI=0 -DHAVE_JPEG
CFLAGS += -I $(WEBRTCROOT)/src -I $(WEBRTCROOT)/src/third_party/jsoncpp/source/include -I $(WEBRTCROOT)/src/third_party/libyuv/include
#detect
TESTDEBUG=$(shell nm $(wildcard $(WEBRTCLIBPATH)/obj/webrtc/rtc_base/librtc_base.a) | c++filt | grep std::__debug::vector >/dev/null && echo debug)
ifeq ($(TESTDEBUG),debug)
	CFLAGS +=-DUSE_DEBUG_WEBRTC -D_GLIBCXX_DEBUG=1
else
	CFLAGS +=-DNDEBUG=1
endif
LDFLAGS += -lX11 -ldl -lrt 

WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj -name '*.a')
WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj/third_party/jsoncpp -name '*.o')
WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj/webrtc/rtc_base -name '*.o')
LIBS+=libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a
libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a: $(WEBRTC_LIB)
	$(AR) -rcT $@ $^

# alsa-lib
ifneq ($(wildcard $(SYSROOT)/usr/include/alsa/asoundlib.h),)
CFLAGS += -DHAVE_ALSA
LDFLAGS+= -lasound
endif


# live555helper
ifneq ($(wildcard $(SYSROOT)/$(PREFIX)/include/liveMedia/liveMedia.hh),)
VERSION+=live555helper@$(shell git -C live555helper describe --tags --always --dirty)
LIBS+=live555helper/live555helper.a
live555helper/Makefile:
	git submodule update --init live555helper

live555helper/live555helper.a: live555helper/Makefile
	git submodule update live555helper
	make -C live555helper CC=$(CXX) PREFIX=$(PREFIX) CFLAGS_EXTRA="$(CFLAGS)"

CFLAGS += -DHAVE_LIVE555
CFLAGS += -I live555helper/inc
CFLAGS += -I $(SYSROOT)/$(PREFIX)/include/liveMedia  -I $(SYSROOT)/$(PREFIX)/include/groupsock -I $(SYSROOT)/$(PREFIX)/include/UsageEnvironment -I $(SYSROOT)/$(PREFIX)/include/BasicUsageEnvironment/

LDFLAGS += live555helper/live555helper.a
LDFLAGS += -L $(SYSROOT)/$(PREFIX)/lib -l:libliveMedia.a -l:libgroupsock.a -l:libUsageEnvironment.a -l:libBasicUsageEnvironment.a 
endif

# civetweb
VERSION+=civetweb@$(shell git -C civetweb describe --tags --always --dirty)
LIBS+=civetweb/libcivetweb.a
civetweb/Makefile:
	git submodule update --init civetweb

civetweb/libcivetweb.a: civetweb/Makefile
	make lib WITH_CPP=1 CXX=$(CXX) CC=$(CC) COPT="$(CFLAGS)" -C civetweb

CFLAGS += -I civetweb/include
LDFLAGS += -L civetweb -l civetweb

#h264bitstream
VERSION+=h264bitstream@$(shell git -C h264bitstream describe --tags --always --dirty)
LIBS+=h264bitstream/.libs/libh264bitstream.a
h264bitstream/Makefile:
	git submodule update --init h264bitstream	

h264bitstream/.libs/libh264bitstream.a: h264bitstream/Makefile
	cd h264bitstream && autoreconf -i -f
	cd h264bitstream && CC=$(CXX) ./configure --host=$(shell $(CXX) -dumpmachine)
	make -C h264bitstream 

CFLAGS += -I h264bitstream
LDFLAGS += h264bitstream/.libs/libh264bitstream.a

src/%.o: src/%.cpp $(LIBS)
	$(CXX) -o $@ -c $< $(CFLAGS) -DVERSION="\"$(VERSION)\""

FILES = $(wildcard src/*.cpp)
$(TARGET): $(subst .cpp,.o,$(FILES)) $(LIBS) 
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f src/*.o libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a $(TARGET)
	make -C civetweb clean
	make -C h264bitstream clean
	make -k -C live555helper clean

install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin

tgz: $(TARGET)
	tar cvzf $(TARGET)_$(GITVERSION)_$(GYP_GENERATOR_OUTPUT).tgz $(TARGET) html

live555:
	wget http://www.live555.com/liveMedia/public/live555-latest.tar.gz -O - | tar xzf -
	cd live && ./genMakefiles linux-gdb
	make -C live CPLUSPLUS_COMPILER="$(CXX) -fno-rtti $(CFLAGS_EXTRA)" C_COMPILER=$(CC) LINK='$(CXX) -o' PREFIX=$(SYSROOT)/$(PREFIX) install
	rm -rf live

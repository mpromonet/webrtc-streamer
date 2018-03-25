CC=$(CROSS)gcc 
CXX=$(CROSS)g++
AR=$(CROSS)ar
CFLAGS = -Wall -pthread -g -std=c++11 -Iinc 
LDFLAGS = -pthread 
WEBRTCROOT?=$(CURDIR)/../webrtc
WEBRTCBUILD?=Release
PREFIX?=/usr
GITVERSION=$(shell git describe --tags --always --dirty)
VERSION=$(GITVERSION)


TARGET = $(notdir $(CURDIR))
all: $(TARGET)

# webrtc
VERSION+=webrtc@$(shell git -C $(WEBRTCROOT)/src describe --tags --always --dirty)
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(GYP_GENERATOR_OUTPUT)/out/$(WEBRTCBUILD)
WEBRTCSYSROOT=$(shell grep -Po 'sysroot=\K[^ ]*' $(WEBRTCLIBPATH)/obj/webrtc_common.ninja)
ifeq ($(WEBRTCSYSROOT),)
	SYSROOT?=$(shell $(CC) -print-sysroot)
else
	SYSROOT?=$(WEBRTCLIBPATH)/$(WEBRTCSYSROOT)
endif
$(info SYSROOT=$(SYSROOT))
SYSROOTOPT=--sysroot=$(SYSROOT)
CFLAGS += $(SYSROOTOPT) $(CFLAGS_EXTRA)
LDFLAGS += $(SYSROOTOPT)

CFLAGS += -DWEBRTC_POSIX -fno-rtti -DHAVE_JPEG
CFLAGS += -I $(WEBRTCROOT)/src -I $(WEBRTCROOT)/src/third_party/jsoncpp/source/include -I $(WEBRTCROOT)/src/third_party/libyuv/include
#detect debug vs release
TESTDEBUG=$(shell nm $(wildcard $(WEBRTCLIBPATH)/obj/rtc_base/librtc_base_generic.a) | c++filt | grep std::__debug::vector >/dev/null && echo debug)
ifeq ($(TESTDEBUG),debug)
	CFLAGS +=-DUSE_DEBUG_WEBRTC -D_GLIBCXX_DEBUG=1
else
	CFLAGS +=-DNDEBUG=1
endif
LDFLAGS += -ldl -lrt 

# desktop capture
ifneq ($(wildcard $(WEBRTCLIBPATH)/obj/modules/desktop_capture/desktop_capture.ninja),)
CFLAGS += -DUSE_X11
LDFLAGS += -lX11 -lXext -lXdamage -lXfixes -lXcomposite
endif

WEBRTC_LIB += $(shell find $(WEBRTCLIBPATH)/obj -name '*.o')
LIBS+=libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a
libWebRTC_$(GYP_GENERATOR_OUTPUT)_$(WEBRTCBUILD).a: $(WEBRTC_LIB)
	$(AR) -rc $@ $^

# alsa-lib
ifneq ($(wildcard $(SYSROOT)$(PREFIX)/include/alsa/asoundlib.h),)
CFLAGS += -DHAVE_ALSA -I $(SYSROOT)$(PREFIX)/include
LDFLAGS+= -L $(SYSROOT)$(PREFIX)/lib -lasound
else	
$(info ALSA not found in $(SYSROOT)$(PREFIX)/include)
endif


# live555helper
ifneq ($(wildcard $(SYSROOT)$(PREFIX)/include/liveMedia/liveMedia.hh),)
VERSION+=live555helper@$(shell git -C live555helper describe --tags --always --dirty)
LIBS+=live555helper/live555helper.a
live555helper/Makefile:
	git submodule update --init live555helper

live555helper/live555helper.a: live555helper/Makefile
	git submodule update live555helper
	make -C live555helper CC=$(CXX) PREFIX=$(PREFIX) CFLAGS_EXTRA="$(CFLAGS)" SYSROOT=$(SYSROOT)

CFLAGS += -DHAVE_LIVE555
CFLAGS += -I live555helper/inc
CFLAGS += -I $(SYSROOT)$(PREFIX)/include/liveMedia  -I $(SYSROOT)$(PREFIX)/include/groupsock -I $(SYSROOT)$(PREFIX)/include/UsageEnvironment -I $(SYSROOT)$(PREFIX)/include/BasicUsageEnvironment/

LDFLAGS += live555helper/live555helper.a
LDFLAGS += -L $(SYSROOT)$(PREFIX)/lib -l:libliveMedia.a -l:libgroupsock.a -l:libUsageEnvironment.a -l:libBasicUsageEnvironment.a 
else	
$(info LIVE555 not found in $(SYSROOT)$(PREFIX)/include)
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
	make -C live CPLUSPLUS_COMPILER="$(CXX) -fno-rtti $(CFLAGS_EXTRA)" C_COMPILER=$(CC) LINK='$(CXX) -o' PREFIX=$(SYSROOT)$(PREFIX) install
	rm -rf live

ALSAVERSION=1.1.4.1
alsa-lib:
	wget ftp://ftp.alsa-project.org/pub/lib/alsa-lib-$(ALSAVERSION).tar.bz2 -O - | tar xjf -
	cd alsa-lib-$(ALSAVERSION) && CC=$(CC) ./configure --disable-python --disable-shared --enable-static --host=$(shell $(CC) -dumpmachine) --prefix=$(SYSROOT)$(PREFIX) && make && make install
	rm -rf alsa-lib-$(ALSAVERSION)

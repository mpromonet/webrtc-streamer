CC = g++
CFLAGS = -W -pthread -g -std=c++11
TARGET=webrtc-server

CFLAGS += -I mongoose

WEBRTCROOT=../src
WEBRTCBUILD=Release
WEBRTCLIBPATH=$(WEBRTCROOT)/out/$(WEBRTCBUILD)

CFLAGS += \
    -DLOGGING=1 \
    -DFEATURE_ENABLE_SSL -DFEATURE_ENABLE_PSTN -DHAVE_SCTP -DHAVE_SRTP \
    -DHAVE_WEBRTC_VIDEO -DHAVE_WEBRTC_VOICE \
    -D_FILE_OFFSET_BITS=64 \
    -DENABLE_REMOTING=1 -DENABLE_WEBRTC=1 \
    -DENABLE_CONFIGURATION_POLICY -DENABLE_NOTIFICATIONS -DUSE_UDEV \
    -DDONT_EMBED_BUILD_METADATA \
    -DLINUX -DWEBRTC_LINUX -DPOSIX -DWEBRTC_POSIX \
    -DDISABLE_DYNAMIC_CAST -D_REENTRANT -DUSE_NSS=1 -DDYNAMIC_ANNOTATIONS_ENABLED=0

CFLAGS += -fstack-protector --param=ssp-buffer-size=4 -Werror -pthread \
    -fno-strict-aliasing -Wno-unused-parameter \
    -Wno-missing-field-initializers -fvisibility=hidden -pipe \
    -Wno-char-subscripts -Wno-format \
    -Wno-unused-result -fno-ident -fdata-sections \
    -ffunction-sections -funwind-tables -fno-rtti
    
CFLAGS += -I $(WEBRTCROOT) -I $(WEBRTCROOT)/chromium/src/third_party/jsoncpp/source/include
LDFLAGS +=\
	-Wl,--start-group \
	$(WEBRTCLIBPATH)/libyuv.a \
	$(WEBRTCLIBPATH)/obj/talk/libjingle_peerconnection.a \
	$(WEBRTCLIBPATH)/obj/talk/libjingle_media.a \
	$(WEBRTCLIBPATH)/obj/talk/libjingle_p2p.a \
	$(WEBRTCLIBPATH)/obj/webrtc/libwebrtc.a \
	$(WEBRTCLIBPATH)/obj/webrtc/libwebrtc_common.a \
	$(WEBRTCLIBPATH)/obj/webrtc/base/librtc_base.a \
	$(WEBRTCLIBPATH)/obj/webrtc/base/librtc_base_approved.a \
	$(WEBRTCLIBPATH)/obj/webrtc/common_audio/libcommon_audio.a \
	$(WEBRTCLIBPATH)/obj/webrtc/common_audio/libcommon_audio_sse2.a \
	$(WEBRTCLIBPATH)/obj/webrtc/common_video/libcommon_video.a \
	$(WEBRTCLIBPATH)/obj/webrtc/libjingle/xmllite/librtc_xmllite.a \
	$(WEBRTCLIBPATH)/obj/webrtc/libjingle/xmpp/librtc_xmpp.a \
	$(WEBRTCLIBPATH)/obj/webrtc/p2p/librtc_p2p.a \
	$(WEBRTCLIBPATH)/obj/webrtc/sound/librtc_sound.a \
	$(WEBRTCLIBPATH)/obj/webrtc/system_wrappers/source/libfield_trial_default.a \
	$(WEBRTCLIBPATH)/obj/webrtc/system_wrappers/source/libmetrics_default.a \
	$(WEBRTCLIBPATH)/obj/webrtc/system_wrappers/source/libsystem_wrappers.a \
	$(WEBRTCLIBPATH)/obj/webrtc/video_engine/libvideo_engine_core.a \
	$(WEBRTCLIBPATH)/obj/webrtc/voice_engine/libvoice_engine.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_render_module.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libwebrtc_utility.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudio_coding_module.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libCNG.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libG711.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libG722.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libiLBC.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libiSAC.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libiSACFix.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libPCM16B.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libwebrtc_opus.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libneteq.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libmedia_file.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libwebrtc_video_coding.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libwebrtc_i420.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/librtp_rtcp.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libpaced_sender.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libremote_bitrate_estimator.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libbitrate_controller.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_capture_module.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_processing.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_processing_sse2.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudio_conference_mixer.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudio_processing.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudioproc_debug_proto.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudio_processing_sse2.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libaudio_device.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_capture_module_internal_impl.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/libvideo_render_module_internal_impl.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/remote_bitrate_estimator/librbe_components.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/video_coding/utility/libvideo_coding_utility.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/video_coding/codecs/vp8/libwebrtc_vp8.a \
	$(WEBRTCLIBPATH)/obj/webrtc/modules/video_coding/codecs/vp9/libwebrtc_vp9.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/net/third_party/nss/libcrssl.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/jsoncpp/libjsoncpp.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libjpeg_turbo/libjpeg_turbo.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/usrsctp/libusrsctplib.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/openmax_dl/dl/libopenmax_dl.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/opus/libopus.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx_asm_offsets_vp8.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx_intrinsics_mmx.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx_intrinsics_sse2.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx_intrinsics_ssse3.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libvpx/libvpx_intrinsics_sse4_1.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/protobuf/libprotobuf_lite.a \
	$(WEBRTCLIBPATH)/obj/chromium/src/third_party/libsrtp/libsrtp.a -Wl,--end-group  \
	-lX11 -lXext -lexpat -ldl -lrt -lnss3 -lnssutil3 -lplc4 -lnspr4 -lm

   
all: $(TARGET)

mongoose/mongoose.c: 
	git submodule init
	git submodule update
	
$(TARGET): main.cc webrtc.cpp mongoose/mongoose.c 
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)	

clean:
	rm -f *.o $(TARGET)

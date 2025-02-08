cmake_minimum_required (VERSION 3.5)
project (webrtc-streamer)

set (WEBRTCROOT "${CMAKE_CURRENT_SOURCE_DIR}/../webrtc" CACHE STRING "WEBRTC root directory")
set (WEBRTCDESKTOPCAPTURE "ON" CACHE STRING "WEBRTC Desktop capture")
set (WEBRTCCHROMEBRANDED "ON" CACHE STRING "WEBRTC Chrome branded")
set (WEBRTCVERSION "df532ba1db7e67dcffe36890dbaf907d1678ffdf" CACHE STRING "WEBRTC version")

if(NOT CMAKE_BUILD_TYPE)
set (CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}\nWEBRTCROOT = ${WEBRTCROOT}\nWEBRTCDESKTOPCAPTURE= ${WEBRTCDESKTOPCAPTURE}\nCMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}\nWEBRTCVERSION=${WEBRTCVERSION}")

set (CMAKE_POSITION_INDEPENDENT_CODE ON)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_EXTENSIONS OFF)


# set CMAKE_FIND_ROOT_PATH to search package in WebRTC sysroot
if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv.*")
  set (CMAKE_FIND_ROOT_PATH ${WEBRTCROOT}/src/build/linux/debian_bullseye_arm-sysroot)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  set (CMAKE_FIND_ROOT_PATH ${WEBRTCROOT}/src/build/linux/debian_bullseye_arm64-sysroot)
  add_compile_options(--sysroot=${CMAKE_FIND_ROOT_PATH})
endif()
MESSAGE("CMAKE_FIND_ROOT_PATH = ${CMAKE_FIND_ROOT_PATH}")

# alsa ?
find_package(ALSA QUIET)
MESSAGE("ALSA_FOUND = ${ALSA_FOUND}")

# pulse ?
find_package(PulseAudio QUIET)
MESSAGE("PulseAudio_FOUND = ${PulseAudio_FOUND}")

set(ENV{PATH} "${WEBRTCROOT}/src/third_party/llvm-build/Release+Asserts/bin:$ENV{PATH}")
MESSAGE("PATH = $ENV{PATH}")

find_package(Git)

# webrtc build
set (WEBRTCOBJS ${WEBRTCROOT}/src/out/${CMAKE_BUILD_TYPE}/obj)

if (NOT EXISTS ${WEBRTCOBJS}/${CMAKE_STATIC_LIBRARY_PREFIX}webrtc${CMAKE_STATIC_LIBRARY_SUFFIX})
  if(GIT_FOUND)
    EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src COMMAND ${GIT_EXECUTABLE} status)
    EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src COMMAND ${GIT_EXECUTABLE} checkout ${WEBRTCVERSION})
  endif()

  file(READ ${WEBRTCROOT}/src/DEPS filecontent)
  string(REPLACE "'src/resources'],$" "'src/resources'],'condition':'rtc_include_tests==true'," filecontent "${filecontent}")
  file(WRITE ${WEBRTCROOT}/src/DEPS "${filecontent}")

  EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src COMMAND gclient sync)

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv.*")
    EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src
                    COMMAND build/linux/sysroot_scripts/install-sysroot.py --arch=arm 
                    RESULT_VARIABLE SYSROOT_RESULT
                    OUTPUT_VARIABLE SYSROOT_OUTPUT
                    ERROR_VARIABLE SYSROOT_ERROR
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_STRIP_TRAILING_WHITESPACE)
    if(NOT SYSROOT_RESULT EQUAL 0)
      message(FATAL_ERROR "install-sysroot.py failed with exit code ${SYSROOT_RESULT}\nOutput:\n${SYSROOT_OUTPUT}\nError:\n${SYSROOT_ERROR}")
    else()
      message("install-sysroot.py succeeded ${SYSROOT_OUTPUT}")
    endif()
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src 
                    COMMAND build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
                    RESULT_VARIABLE SYSROOT_RESULT
                    OUTPUT_VARIABLE SYSROOT_OUTPUT
                    ERROR_VARIABLE SYSROOT_ERROR
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_STRIP_TRAILING_WHITESPACE)
    if(NOT SYSROOT_RESULT EQUAL 0)
      message(FATAL_ERROR "install-sysroot.py failed with exit code ${SYSROOT_RESULT}\nOutput:\n${SYSROOT_OUTPUT}\nError:\n${SYSROOT_ERROR}")
    else()
      message("install-sysroot.py succeeded ${SYSROOT_OUTPUT}")
    endif()
  endif()


  set (WEBRTCARGS rtc_include_tests=false\nrtc_enable_protobuf=false\nrtc_build_examples=false\nrtc_build_tools=false\ntreat_warnings_as_errors=false\nenable_js_protobuf=false\nuse_glib=false\n)
  set (WEBRTCARGS use_custom_libcxx=false\nlibyuv_use_sme=false\n${WEBRTCARGS})
  # debug/release
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set (WEBRTCARGS is_debug=false\n${WEBRTCARGS})
  else()
    set (WEBRTCARGS is_debug=true\n${WEBRTCARGS})
  endif()

  # enable H264 support
  if (WEBRTCCHROMEBRANDED STREQUAL "ON")
    set (WEBRTCARGS is_chrome_branded=true\n${WEBRTCARGS})
  else()
    set (WEBRTCARGS rtc_use_h264=true\nrtc_use_h265=true\n${WEBRTCARGS})
  endif()
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
    set (CONFIGURE_ARGS --disable-everything --disable-all --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-static --enable-avcodec --enable-avformat --enable-avutil --enable-static --enable-libopus --disable-debug --disable-bzlib --disable-iconv --disable-network --disable-schannel --disable-sdl2 --disable-symver --disable-xlib --disable-zlib --disable-securetransport --disable-faan --disable-alsa --disable-autodetect --disable-error-resilience --enable-decoder=vorbis,libopus,flac --enable-decoder=pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,mp3 --enable-decoder=pcm_s16be,pcm_s24be,pcm_mulaw,pcm_alaw --enable-demuxer=ogg,matroska,wav,flac,mp3,mov --enable-parser=opus,vorbis,flac,mpegaudio,vp9 --extra-cflags=-I${WEBRTCROOT}/src/third_party/opus/src/include --disable-linux-perf --x86asmexe=nasm --optflags="-O2" --arch=arm --enable-armv6 --disable-armv6t2 --enable-vfp --disable-thumb --extra-cflags=-march=armv6 --enable-cross-compile --target-os=linux --extra-cflags=--target=arm-linux-gnueabihf --extra-ldflags=--target=arm-linux-gnueabihf --sysroot=${WEBRTCROOT}/src/build/linux/debian_bullseye_armhf-sysroot --extra-cflags=-mtune=cortex-a8 --extra-cflags=-mfloat-abi=hard --extra-cflags=-O2 --disable-neon --extra-cflags=-mfpu=vfpv3-d16 --enable-pic --cc=clang --cxx=clang++ --ld=clang --extra-ldflags=-fuse-ld=lld --enable-decoder=aac,h264 --enable-demuxer=aac --enable-parser=aac,h264)
    MESSAGE("CONFIGURE_ARGS = ${CONFIGURE_ARGS}")
    EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src/third_party/ffmpeg COMMAND ./configure ${CONFIGURE_ARGS})

    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/config.h DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/config_components.h DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavutil/avconfig.h DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavutil/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavfilter/filter_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavutil/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavcodec/codec_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavcodec/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavcodec/parser_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavcodec/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavcodec/bsf_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavcodec/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavformat/demuxer_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavformat/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavformat/muxer_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavformat/)
    file(COPY ${WEBRTCROOT}/src/third_party/ffmpeg/libavformat/protocol_list.c DESTINATION ${WEBRTCROOT}/src/third_party/ffmpeg/chromium/config/Chrome/linux/arm/libavformat/)

  endif()

  #patch
  file(READ ${WEBRTCROOT}/src/media/base/media_channel.h filecontent)
  string(REGEX REPLACE "ost << ToStringIfSet[^;]*;" "" filecontent "${filecontent}")
  file(WRITE ${WEBRTCROOT}/src/media/base/media_channel.h "${filecontent}")

  file(READ ${WEBRTCROOT}/src/call/rtp_config.cc filecontent)
  string(REGEX REPLACE "stream_config.rtx.emplace\\(\\)" "stream_config.rtx.emplace(RtpStreamConfig::Rtx())" filecontent "${filecontent}")
  file(WRITE ${WEBRTCROOT}/src/call/rtp_config.cc "${filecontent}")
  

  # sound support
  if (APPLE)
    set (WEBRTCARGS rtc_include_internal_audio_device=true\n${WEBRTCARGS})
    set (WEBRTCARGS rtc_include_pulse_audio=true\n${WEBRTCARGS})
  elseif (WIN32)
    set (WEBRTCARGS rtc_include_internal_audio_device=true\n${WEBRTCARGS})
  else()
    if (NOT PulseAudio_FOUND)
      set (WEBRTCARGS rtc_include_pulse_audio=false\n${WEBRTCARGS})
    endif()
    if (NOT ALSA_FOUND)
      set (WEBRTCARGS rtc_include_internal_audio_device=false\n${WEBRTCARGS})
    endif()
  endif()

  # compilation mode depending on target
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
    set (WEBRTCARGS target_cpu="arm"\narm_version=6\narm_float_abi="hard"\nenable_libaom=false\n${WEBRTCARGS})
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv.*")
    set (WEBRTCARGS target_cpu="arm"\n${WEBRTCARGS})
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set (WEBRTCARGS target_cpu="arm64"\n${WEBRTCARGS})
  endif()
  if (DEFINED CMAKE_FIND_ROOT_PATH)
    set (WEBRTCARGS use_sysroot=true\n${WEBRTCARGS})
  else()
    set (WEBRTCARGS use_sysroot=false\n${WEBRTCARGS})
  endif()
  set (WEBRTCARGS is_clang=true\n${WEBRTCARGS})

  # screen capture support
  find_package(PkgConfig QUIET)
  pkg_check_modules(GTK3 QUIET gtk+-3.0)
  MESSAGE("GTK3_FOUND = ${GTK3_FOUND}")
  if(NOT GTK3_FOUND OR (WEBRTCDESKTOPCAPTURE STREQUAL "OFF"))
    set (WEBRTCARGS rtc_use_x11=false\nrtc_use_pipewire=false\n${WEBRTCARGS})
  endif()

  # write conf file
  FILE(WRITE ${WEBRTCROOT}/src/out/${CMAKE_BUILD_TYPE}/args.gn ${WEBRTCARGS})

  if (WIN32)
    SET (SHELLCOMMAND cmd /c )
  endif()
  EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${CMAKE_BUILD_TYPE} COMMAND ${SHELLCOMMAND} gn gen .)  

  SET(NINJA_TARGET webrtc rtc_json builtin_video_decoder_factory builtin_video_encoder_factory p2p_server_utils task_queue default_task_queue_factory)
  EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${CMAKE_BUILD_TYPE} COMMAND ${SHELLCOMMAND} ninja ${NINJA_TARGET} RESULT_VARIABLE NINJA_RESULT)
  if(NOT NINJA_RESULT EQUAL 0)
    message(FATAL_ERROR "webrtc build failed with exit code ${NINJA_RESULT}")
  endif()  
endif()

# build version identifier
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} submodule update --init)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE PROJECTVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${PROJECTVERSION}/${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C civetweb describe --tags --always --dirty OUTPUT_VARIABLE CIVETVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} civetweb@${CIVETVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C ${WEBRTCROOT}/src describe --tags --always --dirty OUTPUT_VARIABLE WEBRTCVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} webrtc@${WEBRTCVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C live555helper describe --tags --always --dirty OUTPUT_VARIABLE LIVEVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} live555helper@${LIVEVERSION}")
endif()
add_definitions(-DVERSION=\"${VERSION}\")
MESSAGE("VERSION = ${VERSION}")

# project target
FILE(GLOB_RECURSE WEBRTJSONCPPCOBJS ${WEBRTCOBJS}/third_party/jsoncpp/jsoncpp/*${CMAKE_C_OUTPUT_EXTENSION} ${WEBRTCOBJS}/rtc_base/rtc_json/*${CMAKE_C_OUTPUT_EXTENSION})
FILE(GLOB_RECURSE WEBRTP2POBJ ${WEBRTCOBJS}/p2p/p2p_server_utils/*${CMAKE_C_OUTPUT_EXTENSION})
SET (WEBRTCEXTRAOBJS ${WEBRTJSONCPPCOBJS} ${WEBRTP2POBJ} ${WEBRTCCOMOBJ})	
FILE(GLOB SOURCE src/*.cpp)
add_executable (${CMAKE_PROJECT_NAME} ${SOURCE} ${WEBRTCEXTRAOBJS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE inc) 

# webrtc 
set (WEBRTCINCLUDE ${WEBRTCROOT}/src ${WEBRTCROOT}/src/third_party/abseil-cpp ${WEBRTCROOT}/src/third_party/jsoncpp/source/include  ${WEBRTCROOT}/src/third_party/jsoncpp/generated ${WEBRTCROOT}/src/third_party/libyuv/include)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${WEBRTCINCLUDE})
set(WEBRTC_LIBS_INFO
    "${WEBRTCOBJS}/api/video_codecs/${CMAKE_STATIC_LIBRARY_PREFIX}builtin_video_encoder_factory${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${WEBRTCOBJS}/api/video_codecs/${CMAKE_STATIC_LIBRARY_PREFIX}builtin_video_decoder_factory${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${WEBRTCOBJS}/media/${CMAKE_STATIC_LIBRARY_PREFIX}rtc_internal_video_codecs${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${WEBRTCOBJS}/media/${CMAKE_STATIC_LIBRARY_PREFIX}rtc_simulcast_encoder_adapter${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${WEBRTCOBJS}/${CMAKE_STATIC_LIBRARY_PREFIX}webrtc${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
foreach(LIB ${WEBRTC_LIBS_INFO})
    target_link_libraries(${CMAKE_PROJECT_NAME} ${LIB})
endforeach()
add_definitions(-DHAVE_JPEG)

# thread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads)
target_link_libraries (${CMAKE_PROJECT_NAME} Threads::Threads) 

# civetweb
add_library (civetweb STATIC civetweb/src/civetweb.c civetweb/src/CivetServer.cpp)	
add_definitions(-DOPENSSL_API_3_0 -DUSE_WEBSOCKET)
target_link_libraries (${CMAKE_PROJECT_NAME} civetweb) 
target_include_directories(civetweb PUBLIC civetweb/include) 

# rtmp ?
find_package(PkgConfig QUIET)
pkg_check_modules(RTMP QUIET librtmp)
MESSAGE("RTMP_FOUND = ${RTMP_FOUND} RTMP_INCLUDE_DIRS=${RTMP_INCLUDE_DIRS} RTMP_LIBRARY_DIRS=${RTMP_LIBRARY_DIRS} RTMP_LIBRARIES=${RTMP_LIBRARIES}")
if (RTMP_FOUND)
  add_definitions(-DHAVE_RTMP)
  target_include_directories (${CMAKE_PROJECT_NAME} PUBLIC ${RTMP_INCLUDE_DIRS}) 
  target_link_directories (${CMAKE_PROJECT_NAME} PUBLIC ${RTMP_LIBRARY_DIRS})
  find_library(RTMP_LIB rtmp)
  MESSAGE("RTMP_LIB=${RTMP_LIB}")
  target_link_libraries (${CMAKE_PROJECT_NAME} ${RTMP_LIB}) 
endif()

# compiler specific
if (WIN32)
    # overide compilation flags
    set(CompilerFlags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    # getopt
    add_library (getopt getopt/getopt.c)
    target_include_directories(getopt PUBLIC getopt) 
    target_link_libraries (${CMAKE_PROJECT_NAME} getopt)

    # webrtc
    add_definitions(-DWEBRTC_WIN -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__ -D_HAS_ITERATOR_DEBUGGING=0)
    add_definitions(-DUSE_X11 -DHAVE_SOUND -D_WINSOCKAPI_)
    target_link_libraries (${CMAKE_PROJECT_NAME} secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm dxgi d3d11 iphlpapi dwmapi shcore)
elseif (APPLE)
    # use lld to link
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -B${WEBRTCROOT}/src/third_party/llvm-build/Release+Asserts/bin")

    # live555
    add_definitions(-DNEED_XLOCALE_H=1)
    # webrtc
    add_definitions(-DHAVE_SOUND)
    add_definitions(-DWEBRTC_MAC -DWEBRTC_POSIX -fno-rtti)

    find_library(CORE_FOUNDATION Foundation)
    find_library(APPLICATION_SERVICES ApplicationServices)
    find_library(CORE_SERVICES CoreServices)
    find_library(CORE_AUDIO CoreAudio)
    find_library(AUDIO_TOOLBOX AudioToolBox)
    find_library(IO_SURFACE IOSurface)
    find_library(APP_KIT AppKit)
    
    
    target_link_libraries (${CMAKE_PROJECT_NAME} ${CORE_FOUNDATION} ${APPLICATION_SERVICES} ${CORE_SERVICES}
                           ${CORE_AUDIO} ${AUDIO_TOOLBOX} ${IO_SURFACE} ${APP_KIT})
else()
    # use lld to link
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -B${WEBRTCROOT}/src/third_party/llvm-build/Release+Asserts/bin")

    # libv4l2cpp
    add_definitions(-DHAVE_V4L2)
    aux_source_directory(libv4l2cpp/src LIBSRC_FILES)
    include_directories("libv4l2cpp/inc")
    add_library(libv4l2cpp STATIC ${LIBSRC_FILES})
    target_link_libraries (${CMAKE_PROJECT_NAME} libv4l2cpp) 

    # webrtc
    add_definitions(-DWEBRTC_POSIX -fno-rtti)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
        add_definitions(-marm -march=armv6 -mfpu=vfp -mfloat-abi=hard)
    endif()
    if (EXISTS ${WEBRTCROOT}/src/out/${CMAKE_BUILD_TYPE}/obj/modules/desktop_capture/desktop_capture.ninja)
        add_definitions(-DUSE_X11)
        find_package(X11)
        target_link_libraries(${CMAKE_PROJECT_NAME} ${X11_LIBRARIES} ${X11_Xdamage_LIB} ${X11_Xfixes_LIB} ${X11_Xcomposite_LIB} ${X11_Xrandr_LIB} ${X11_Xtst_LIB})
    endif()
    # civetweb
    target_link_libraries(${CMAKE_PROJECT_NAME} dl)    
    #alsa
    if (ALSA_FOUND)
        add_definitions(-DHAVE_SOUND)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Compiler is Clang CMAKE_LIBRARY_ARCHITECTURE:${CMAKE_LIBRARY_ARCHITECTURE} CMAKE_SYSTEM_PROCESSOR:${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv.*")
      add_definitions(-target arm-linux-gnueabihf)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      if (NOT APPLE)
        add_definitions(-target aarch64-linux-gnu)
      else ()
        add_definitions(-target aarch64-apple-macos)
      endif()  
    endif()
endif()

# prometheus 
include(GenerateExportHeader)
include(GNUInstallDirs)
set(PROJECT_NAME prometheus-cpp)
add_subdirectory(prometheus-cpp/core EXCLUDE_FROM_ALL)
target_link_libraries (${CMAKE_PROJECT_NAME} core)  
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE prometheus-cpp/core/include)

# live555helper 
include_directories(${WEBRTCROOT}/src/third_party/boringssl/src/include)
add_subdirectory(live555helper EXCLUDE_FROM_ALL)
target_link_libraries (${CMAKE_PROJECT_NAME} liblive555helper ${WEBRTCOBJS}/third_party/boringssl/${CMAKE_STATIC_LIBRARY_PREFIX}boringssl${CMAKE_STATIC_LIBRARY_SUFFIX}) 
add_definitions(-DHAVE_LIVE555)

# static link of libatomic on armv6l
if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
    target_link_libraries (${CMAKE_PROJECT_NAME} -Wl,-Bstatic -latomic -Wl,-Bdynamic)
endif()
# static link of stdc++ if available
if (NOT APPLE)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-static-libgcc" CXX_SUPPORTS_STATIC_LIBGCC)
  if (CXX_SUPPORTS_STATIC_LIBGCC) 
    target_link_libraries (${CMAKE_PROJECT_NAME} -static-libgcc)
  endif()    
  CHECK_CXX_COMPILER_FLAG("-static-libstdc++" CXX_SUPPORTS_STATIC_LIBSTDCPP)
  if (CXX_SUPPORTS_STATIC_LIBSTDCPP) 
    target_link_libraries (${CMAKE_PROJECT_NAME} -static-libstdc++)
  endif()
endif()

#cpack
install (TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION bin)
install (DIRECTORY html DESTINATION share/${CMAKE_PROJECT_NAME})
install (FILES config.json DESTINATION share/${CMAKE_PROJECT_NAME})
SET(CPACK_GENERATOR "TGZ")
SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE})
SET(CPACK_PACKAGE_CONTACT "michel.promonet@free.fr") 
if(PROJECTVERSION)
    SET(CPACK_PACKAGE_VERSION "${PROJECTVERSION}")
endif()
INCLUDE(CPack)

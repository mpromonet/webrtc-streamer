cmake_minimum_required (VERSION 3.0.0)
project (webrtc-streamer)

set (WEBRTCROOT "../webrtc" CACHE STRING "WEBRTC root directory")
set (WEBRTCBUILD "Release" CACHE STRING "WEBRTC build type")
set (WEBRTCDESKTOPCAPTURE "ON" CACHE STRING "WEBRTC Desktop capture")

MESSAGE("WEBRTCBUILD = ${WEBRTCBUILD}\nWEBRTCROOT = ${WEBRTCROOT}\nWEBRTCDESKTOPCAPTURE= ${WEBRTCDESKTOPCAPTURE}")

set (CMAKE_BUILD_TYPE ${WEBRTCBUILD}) 
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
set (CMAKE_CXX_STANDARD 11)

# build version identifier
find_package(Git)
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} submodule update --init)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE PROJECTVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${PROJECTVERSION}")
    
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C civetweb describe --tags --always --dirty OUTPUT_VARIABLE CIVETVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} civetweb@${CIVETVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C ${WEBRTCROOT}/src describe --tags --always --dirty OUTPUT_VARIABLE WEBRTCVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} webrtc@${WEBRTCVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C live555helper describe --tags --always --dirty OUTPUT_VARIABLE LIVEVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (VERSION "${VERSION} live555helper@${LIVEVERSION}")
endif()
add_definitions(-DVERSION=\"${VERSION}\")

# target
FILE(GLOB SOURCE src/*.cpp)
add_executable (${PROJECT_NAME} ${SOURCE})
target_include_directories(${PROJECT_NAME} PRIVATE inc) 

# thread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads)
target_link_libraries (${PROJECT_NAME} Threads::Threads) 

# civetweb
add_library (civetweb STATIC civetweb/src/civetweb.c civetweb/src/CivetServer.cpp)	
target_link_libraries (${PROJECT_NAME} civetweb) 
target_include_directories(civetweb PUBLIC civetweb/include) 

find_package(ALSA QUIET)
MESSAGE("ALSA_FOUND = ${ALSA_FOUND}")

# webrtc
if (NOT EXISTS ${WEBRTCROOT}/src/out/${WEBRTCBUILD}/args.gn)
  set (WEBRTCARGS rtc_include_tests=false\nrtc_enable_protobuf=false\nuse_custom_libcxx=false\nrtc_include_pulse_audio=false\nrtc_build_examples=false\nrtc_build_tools=false\ntreat_warnings_as_errors=false\n)

  # debug/release
  if(WEBRTCBUILD STREQUAL "Release")
    set (WEBRTCARGS is_debug=false\n${WEBRTCARGS})
  else()
    set (WEBRTCARGS is_debug=true\n${WEBRTCARGS})
  endif()

  # H264 support
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(WARNING "Visual Studio Compiler is not supported by the Chromium project! Disabling h.264.")
    set (WEBRTCARGS rtc_use_h264=false\n${WEBRTCARGS})
  else ()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
      set (WEBRTCARGS rtc_use_h264=false\n${WEBRTCARGS})
      message(WARNING "Disabling h.264.")
    else ()
      set (WEBRTCARGS rtc_use_h264=true\nffmpeg_branding="Chrome"\n${WEBRTCARGS})
    endif()
  endif()

  # sound support
  if (NOT WIN32)
    if (NOT ALSA_FOUND)
      set (WEBRTCARGS rtc_include_internal_audio_device=false\n${WEBRTCARGS})
    endif()
  endif()

  # compilation mode depending on target
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
    set (WEBRTCARGS target_cpu="arm"\narm_version=6\narm_float_abi="hard"\nrtc_include_opus=false\n${WEBRTCARGS})
    set (WEBRTCARGS use_sysroot=false\n${WEBRTCARGS})
    set (WEBRTCARGS is_clang=false\n${WEBRTCARGS})
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*")
    set (WEBRTCARGS target_cpu="arm"\n${WEBRTCARGS})
    set (WEBRTCARGS use_sysroot=true\n${WEBRTCARGS})
    set (WEBRTCARGS is_clang=true\n${WEBRTCARGS})
  else()
    set (WEBRTCARGS use_sysroot=false\n${WEBRTCARGS})
    set (WEBRTCARGS is_clang=true\n${WEBRTCARGS})
  endif()

  # screen capture support
  find_package(PkgConfig QUIET)
  pkg_check_modules(GTK3 QUIET gtk+-3.0)
  MESSAGE("GTK_FOUND = ${GTK3_FOUND}")
  if(NOT GTK3_FOUND OR (WEBRTCDESKTOPCAPTURE STREQUAL "OFF"))
    set (WEBRTCARGS use_ozone=true\nis_desktop_linux=false\n${WEBRTCARGS})
  endif()

  # write conf file
  FILE(WRITE ${WEBRTCROOT}/src/out/${WEBRTCBUILD}/args.gn ${WEBRTCARGS})

  if (WIN32)
    SET (SHELLCOMMAND cmd /c )
  endif(WIN32)

  EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${WEBRTCBUILD} COMMAND ${SHELLCOMMAND} gn gen .)
  
  SET(NINJA_TARGET webrtc rtc_json jsoncpp builtin_video_decoder_factory builtin_video_encoder_factory peerconnection p2p_server_utils)
  EXECUTE_PROCESS(ERROR_QUIET WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${WEBRTCBUILD} COMMAND ninja ${NINJA_TARGET})
  EXECUTE_PROCESS(ERROR_QUIET WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${WEBRTCBUILD} COMMAND ninja ${NINJA_TARGET})
  EXECUTE_PROCESS(ERROR_QUIET WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${WEBRTCBUILD} COMMAND ninja ${NINJA_TARGET})
  EXECUTE_PROCESS(WORKING_DIRECTORY ${WEBRTCROOT}/src/out/${WEBRTCBUILD} COMMAND ninja ${NINJA_TARGET})
endif() 

set (WEBRTCINCLUDE ${WEBRTCROOT}/src ${WEBRTCROOT}/src/third_party/abseil-cpp ${WEBRTCROOT}/src/third_party/jsoncpp/source/include ${WEBRTCROOT}/src/third_party/libyuv/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${WEBRTCINCLUDE})

set (WEBRTCOBJS ${WEBRTCROOT}/src/out/${WEBRTCBUILD}/obj)

FILE(GLOB_RECURSE WEBRTBBRCOBJS ${WEBRTCOBJS}/modules/congestion_controller/bbr/*${CMAKE_C_OUTPUT_EXTENSION})
FILE(GLOB_RECURSE WEBRTJSONCPPCOBJS ${WEBRTCOBJS}/third_party/jsoncpp/jsoncpp/*${CMAKE_C_OUTPUT_EXTENSION})
FILE(GLOB_RECURSE WEBRTP2POBJ ${WEBRTCOBJS}/p2p/p2p_server_utils/*${CMAKE_C_OUTPUT_EXTENSION})
SET (WEBRTCEXTRAOBJS ${WEBRTJSONCPPCOBJS} ${WEBRTBBRCOBJS} ${WEBRTP2POBJ} ${WEBRTCOBJS}/rtc_base/rtc_json/json${CMAKE_C_OUTPUT_EXTENSION})	

find_library(WEBRTC_LIBRARY NAMES webrtc PATHS ${WEBRTCOBJS})
target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY}) 

add_library (webrtcextra STATIC ${WEBRTCEXTRAOBJS})
set_target_properties(webrtcextra PROPERTIES LINKER_LANGUAGE CXX)		
target_link_libraries (${PROJECT_NAME} webrtcextra) 


# live555helper & live555
set(LIVE ${CMAKE_BINARY_DIR}/live)
if (NOT EXISTS ${LIVE})
    message("Download live555!")
    file (DOWNLOAD https://download.videolan.org/pub/contrib/live555/live.2019.03.06.tar.gz ${CMAKE_BINARY_DIR}/live555.tar.gz )
    EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E tar xvf ${CMAKE_BINARY_DIR}/live555.tar.gz  RESULT_VARIABLE unpack_result)
    if(NOT unpack_result STREQUAL "0")
        message(FATAL_ERROR "Fetching live555 failed!")
    endif()
else() 
    message("Use existing live555!")
endif() 

FILE(GLOB LIVEHELPERSOURCE ${LIVE}/groupsock/*.c* ${LIVE}/liveMedia/*.c* ${LIVE}/UsageEnvironment/*.c* ${LIVE}/BasicUsageEnvironment/*.c* live555helper/src/*.cpp)
add_library (live555helper STATIC ${LIVEHELPERSOURCE})	
target_include_directories(live555helper PUBLIC ${LIVE}/groupsock/include ${LIVE}/liveMedia/include ${LIVE}/UsageEnvironment/include ${LIVE}/BasicUsageEnvironment/include live555helper/inc)
target_link_libraries (${PROJECT_NAME} live555helper) 
add_definitions(-DHAVE_LIVE555 -DHAVE_JPEG)

# compiler specific
if (WIN32)
    # live555
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    target_link_libraries (${PROJECT_NAME} ws2_32)
    # getopt
    add_library (getopt getopt/getopt.c)
    target_include_directories(getopt PUBLIC getopt) 
    target_link_libraries (${PROJECT_NAME} getopt)
    # webrtc
    add_definitions(-DWEBRTC_WIN -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__ -D_HAS_ITERATOR_DEBUGGING=0)
    set(CompilerFlags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    add_definitions(-DUSE_X11 -DHAVE_SOUND)
    target_link_libraries (${PROJECT_NAME} secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm dxgi d3d11)
else()
    # live555
    add_definitions(-DBSD=1 -DSOCKLEN_T=socklen_t -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1)
    # webrtc
    add_definitions(-DWEBRTC_POSIX -fno-rtti)
    target_link_libraries (${PROJECT_NAME} dl -static-libgcc -static-libstdc++)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6.*")
        target_link_libraries (${PROJECT_NAME} -Wl,-Bstatic -lm -latomic -Wl,-Bdynamic)
        add_definitions(-marm -march=armv6 -mfpu=vfp -mfloat-abi=hard)
    endif()
    if (EXISTS ${WEBRTCROOT}/src/out/${WEBRTCBUILD}/obj/modules/desktop_capture/desktop_capture.ninja)
        add_definitions(-DUSE_X11)
        target_link_libraries (${PROJECT_NAME} X11 Xext Xdamage Xfixes Xcomposite)
    endif()
    if (ALSA_FOUND)
        add_definitions(-DHAVE_SOUND)
    endif()
endif()

#cpack
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install (DIRECTORY html DESTINATION .)
install (FILES config.json DESTINATION .)
install (FILES Procfile DESTINATION .)
SET(CPACK_GENERATOR "TGZ")
SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE})
SET(CPACK_PACKAGE_CONTACT "michel.promonet@free.fr") 
if(PROJECTVERSION)
    SET(CPACK_PACKAGE_VERSION "${PROJECTVERSION}")
endif()
INCLUDE(CPack)

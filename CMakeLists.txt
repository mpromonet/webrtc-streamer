cmake_minimum_required (VERSION 3.0.0)
 
project (webrtc-streamer)

set (CMAKE_BUILD_TYPE Release) 
set (CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Git)
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REGEX REPLACE "^v(.*)" "\\1" VERSION "${VERSION}")
endif()
add_definitions("-DHAVE_JPEG -DVERSION=\\\"${VERSION}\\\"")

# source & objects
set (SOURCE civetweb/src/civetweb.c civetweb/src/CivetServer.cpp src/HttpServerRequestHandler.cpp src/PeerConnectionManager.cpp src/rtspaudiocapturer.cpp src/rtspvideocapturer.cpp src/main.cpp)
if (MSVC)	
    set (SOURCE getopt/getopt.c ${SOURCE})
endif(MSVC)

# live555 & live555helper
find_path(LIVE555_DIR live)
if (NOT LIVE555_DIR)
		file (DOWNLOAD http://www.live555.com/liveMedia/public/live555-latest.tar.gz ${CMAKE_SOURCE_DIR}/live555-latest.tar.gz )
        EXECUTE_PROCESS(COMMAND 7z x live555-latest.tar.gz -so COMMAND 7z x -ttar -si)
endif(NOT LIVE555_DIR) 

FILE(GLOB groupsockSources live/groupsock/*.c*)
FILE(GLOB liveMediaSources live/liveMedia/*.c*)
FILE(GLOB UsageEnvironmentSources live/UsageEnvironment/*.c*)
FILE(GLOB BasicUsageEnvironmentSources live/BasicUsageEnvironment/*.c*)
set (LIVEINCLUDE live/groupsock/include live/liveMedia/include live/UsageEnvironment/include live/BasicUsageEnvironment/include)
set (SOURCE ${SOURCE} ${groupsockSources} ${liveMediaSources} ${UsageEnvironmentSources} ${BasicUsageEnvironmentSources})
if (MSVC)	
    set (LIVELIBS ws2_32.lib)
endif(MSVC)

set (SOURCE ${SOURCE} live555helper/src/environment.cpp live555helper/src/SessionSink.cpp live555helper/src/rtspconnectionclient.cpp)
set (LIVEINCLUDE ${LIVEINCLUDE}  live555helper/inc)

# webrtc
set (WEBRTCROOT ../webrtc/src)
set (WEBRTCINCLUDE ${WEBRTCROOT} ${WEBRTCROOT}/third_party/abseil-cpp ${WEBRTCROOT}/third_party/jsoncpp/source/include ${WEBRTCROOT}/third_party/libyuv/include)
set (OBJS ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_value${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_reader${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_writer${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/rtc_base/rtc_json/json${CMAKE_C_OUTPUT_EXTENSION})
find_library(WEBRTC_LIBRARY NAMES webrtc PATHS ${WEBRTCROOT}/out/Release/obj/)

# target
add_executable (${PROJECT_NAME} ${SOURCE} ${OBJS})
	
# libs
find_package (Threads)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT}) 

if (MSVC)
	include_directories(inc civetweb/include  getopt ${WEBRTCINCLUDE} ${LIVEINCLUDE}) 
	add_definitions("-DHAVE_LIVE555 -DWEBRTC_WIN  -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__")

	set(CompilerFlags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
	foreach(CompilerFlag ${CompilerFlags})
	  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	  MESSAGE("${CompilerFlag} = ${${CompilerFlag}}")
	endforeach()

	target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY} secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm ${LIVELIBS})
else(MSVC)
	include_directories(inc civetweb/include ${WEBRTCINCLUDE} ${LIVEINCLUDE}) 
	add_definitions("-DHAVE_LIVE555 -DWEBRTC_POSIX -std=c++11 -fno-rtti")
	target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY} dl X11)
endif(MSVC)


cmake_minimum_required (VERSION 3.0.0)
 
project (webrtc-streamer)

find_package(Git)
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REGEX REPLACE "^v(.*)" "\\1" VERSION "${VERSION}")
endif()
add_definitions("-DHAVE_JPEG -DVERSION=\\\"${VERSION}\\\"")

set (CMAKE_BUILD_TYPE Release) 
set (CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# source & objects
set (SOURCE civetweb/src/civetweb.c civetweb/src/CivetServer.cpp src/HttpServerRequestHandler.cpp src/PeerConnectionManager.cpp src/main.cpp)
if (MSVC)
    set (SOURCE getopt/getopt.c ${SOURCE})
endif(MSVC)

set (OBJS ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_value${CMAKE_C_OUTPUT_EXTENSION}
            ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_reader${CMAKE_C_OUTPUT_EXTENSION}
            ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_writer${CMAKE_C_OUTPUT_EXTENSION}
            ../webrtc/src/out/Release/obj/rtc_base/rtc_json/json${CMAKE_C_OUTPUT_EXTENSION})

add_executable (${PROJECT_NAME} ${SOURCE} ${OBJS})

# libs
find_package (Threads)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT}) 

if (MSVC)
	include_directories(inc civetweb/include  getopt ../webrtc/src ../webrtc/src/third_party/abseil-cpp ../webrtc/src/third_party/jsoncpp/source/include) 
	add_definitions("-DWEBRTC_WIN  -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__")

	set(CompilerFlags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
	foreach(CompilerFlag ${CompilerFlags})
	  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	  MESSAGE("${CompilerFlag} = ${${CompilerFlag}}")
	endforeach()

	target_link_libraries (${PROJECT_NAME} ../webrtc/src/out/Release/obj/webrtc secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm)
else(MSVC)
	include_directories(inc civetweb/include ../webrtc/src ../webrtc/src/third_party/abseil-cpp ../webrtc/src/third_party/jsoncpp/source/include) 
	add_definitions("-DWEBRTC_POSIX -std=c++11 -fno-rtti")
	find_library(WEBRTC_LIBRARY NAMES libwebrtc.a PATHS ../webrtc/src/out/Release/obj/)
	target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY} dl X11)
endif(MSVC)


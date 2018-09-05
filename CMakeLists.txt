cmake_minimum_required (VERSION 3.0.0)
 
project (webrtc-streamer)

# add by sunghyun kim
STRING(REGEX REPLACE "\\\\" "/" WEBRTC_HOME "$ENV{WEBRTC_HOME}")
STRING(REGEX REPLACE "\\\\" "/" LIVE555_HOME "$ENV{LIVE555_HOME}")
set (WEBRTCROOT ${WEBRTC_HOME})
set (LIVE555ROOT ${LIVE555_HOME})

set (CMAKE_BUILD_TYPE Release) 
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# build version identifier
find_package(Git)
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REGEX REPLACE "^v(.*)" "\\1" VERSION "${VERSION}")
	
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C civetweb describe --tags --always --dirty OUTPUT_VARIABLE CIVETVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REGEX REPLACE "^v(.*)" "\\1" CIVETVERSION "${CIVETVERSION}")
	set (VERSION "${VERSION} civetweb@${CIVETVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C ${WEBRTCROOT} describe --tags --always --dirty OUTPUT_VARIABLE WEBRTCVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REGEX REPLACE "^v(.*)" "\\1" WEBRTCVERSION "${WEBRTCVERSION}")
	set (VERSION "${VERSION} webrtc@${WEBRTCVERSION}")

endif()
add_definitions(-DVERSION=\"${VERSION}\")

# source & objects
FILE(GLOB SOURCE src/*.cpp)
if (MSVC)	
    set (SOURCE getopt/getopt.c ${SOURCE})
endif(MSVC)

# civetweb sources
set (SOURCE ${SOURCE} civetweb/src/civetweb.c civetweb/src/CivetServer.cpp)

# live555 - modify by sunghyun kim
set (LIVEINCLUDE ${LIVE555_HOME}/groupsock/include ${LIVE555_HOME}/liveMedia/include ${LIVE555_HOME}/UsageEnvironment/include ${LIVE555_HOME}/BasicUsageEnvironment/include)
set (LIVELIB ${LIVE555_HOME}/groupsock/libgroupsock.lib ${LIVE555_HOME}/liveMedia/libliveMedia.lib ${LIVE555_HOME}/UsageEnvironment/libUsageEnvironment.lib ${LIVE555_HOME}/BasicUsageEnvironment/libBasicUsageEnvironment.lib)
set (SOURCE ${SOURCE} live555helper/src/environment.cpp live555helper/src/SessionSink.cpp live555helper/src/rtspconnectionclient.cpp)

# target
add_executable (${PROJECT_NAME} ${SOURCE})
target_include_directories(${PROJECT_NAME} PRIVATE inc civetweb/include ${LIVEINCLUDE} live555helper/inc) 

# thread
find_package (Threads)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT}) 

# webrtc
set (WEBRTCINCLUDE ${WEBRTCROOT} ${WEBRTCROOT}/third_party/abseil-cpp ${WEBRTCROOT}/third_party/jsoncpp/source/include ${WEBRTCROOT}/third_party/libyuv/include)
add_definitions(-DHAVE_LIVE555 -DHAVE_JPEG -DUSE_X11)
target_include_directories(${PROJECT_NAME} PRIVATE ${WEBRTCINCLUDE})

set (OBJS ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_value${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_reader${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_writer${CMAKE_C_OUTPUT_EXTENSION}
			${WEBRTCROOT}/out/Release/obj/rtc_base/rtc_json/json${CMAKE_C_OUTPUT_EXTENSION})

add_library (webrtcextra STATIC ${OBJS} )	
set_target_properties(webrtcextra PROPERTIES LINKER_LANGUAGE CXX)		
find_library(WEBRTC_LIBRARY NAMES webrtc PATHS ${WEBRTCROOT}/out/Release/obj/)
target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY} ${LIVELIB} webrtcextra) 

# compiler specific
if (MSVC)
	# live555
	target_link_libraries (${PROJECT_NAME} ws2_32)
	# getopt
	target_include_directories(${PROJECT_NAME} PRIVATE getopt) 
	# webrtc
	add_definitions(-DWEBRTC_WIN  -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__)
	set(CompilerFlags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
	foreach(CompilerFlag ${CompilerFlags})
	  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	  MESSAGE("${CompilerFlag} = ${${CompilerFlag}}")
	endforeach()
	target_link_libraries (${PROJECT_NAME} secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm dxgi d3d11)
else(MSVC)
	# live555
	add_definitions(-DBSD=1 -DSOCKLEN_T=socklen_t -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1)
	# webrtc
	add_definitions(-DWEBRTC_POSIX -std=c++11 -fno-rtti)
	target_link_libraries (${PROJECT_NAME} dl X11 Xext Xdamage Xfixes Xcomposite)
endif(MSVC)


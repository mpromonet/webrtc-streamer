cmake_minimum_required (VERSION 3.0.0)
 
project (webrtc-streamer)

set (WEBRTCROOT "../webrtc/src" CACHE STRING "WEBRTC root directory")

set (CMAKE_BUILD_TYPE Release) 
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# build version identifier
find_package(Git)
if(GIT_FOUND)
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE PROJECTVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REGEX REPLACE "^v(.*)" "\\1" PROJECTVERSION "${PROJECTVERSION}")
	set (VERSION "${PROJECTVERSION}")
	
    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C civetweb describe --tags --always --dirty OUTPUT_VARIABLE CIVETVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REGEX REPLACE "^v(.*)" "\\1" CIVETVERSION "${CIVETVERSION}")
	set (VERSION "${VERSION} civetweb@${CIVETVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C ${WEBRTCROOT} describe --tags --always --dirty OUTPUT_VARIABLE WEBRTCVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    STRING(REGEX REPLACE "^v(.*)" "\\1" WEBRTCVERSION "${WEBRTCVERSION}")
	set (VERSION "${VERSION} webrtc@${WEBRTCVERSION}")

    EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} -C live555helper describe --tags --always --dirty OUTPUT_VARIABLE LIVEVERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	STRING(REGEX REPLACE "^v(.*)" "\\1" LIVEVERSION "${LIVEVERSION}")
	set (VERSION "${VERSION} live555helper@${LIVEVERSION}")
endif()
add_definitions(-DVERSION=\"${VERSION}\")

# target
FILE(GLOB SOURCE src/*.cpp)
add_executable (${PROJECT_NAME} ${SOURCE})
target_include_directories(${PROJECT_NAME} PRIVATE inc) 

# thread
find_package (Threads)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT}) 

# civetweb
add_library (civetweb STATIC civetweb/src/civetweb.c civetweb/src/CivetServer.cpp)	
target_link_libraries (${PROJECT_NAME} civetweb) 
target_include_directories(civetweb PUBLIC civetweb/include) 

# webrtc
set (WEBRTCINCLUDE ${WEBRTCROOT} ${WEBRTCROOT}/third_party/abseil-cpp ${WEBRTCROOT}/third_party/jsoncpp/source/include ${WEBRTCROOT}/third_party/libyuv/include)
add_definitions(-DHAVE_JPEG -DUSE_X11)
target_include_directories(${PROJECT_NAME} PRIVATE ${WEBRTCINCLUDE})

set (OBJS ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_value${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_reader${CMAKE_C_OUTPUT_EXTENSION}
            ${WEBRTCROOT}/out/Release/obj/third_party/jsoncpp/jsoncpp/json_writer${CMAKE_C_OUTPUT_EXTENSION}
			${WEBRTCROOT}/out/Release/obj/rtc_base/rtc_json/json${CMAKE_C_OUTPUT_EXTENSION})
add_library (webrtcextra STATIC ${OBJS})	
set_target_properties(webrtcextra PROPERTIES LINKER_LANGUAGE CXX)		
find_library(WEBRTC_LIBRARY NAMES webrtc PATHS ${WEBRTCROOT}/out/Release/obj/)
target_link_libraries (${PROJECT_NAME} ${WEBRTC_LIBRARY} webrtcextra) 

# live555helper & live555
if (NOT EXISTS live)
	file (DOWNLOAD http://www.live555.com/liveMedia/public/live555-latest.tar.gz ${CMAKE_SOURCE_DIR}/live555-latest.tar.gz )
    EXECUTE_PROCESS(COMMAND 7z x live555-latest.tar.gz -so COMMAND 7z x -aoa -ttar -si)
endif(NOT EXISTS live) 

FILE(GLOB LIVEHELPERSOURCE live/groupsock/*.c* live/liveMedia/*.c* live/UsageEnvironment/*.c* live/BasicUsageEnvironment/*.c* live555helper/src/*.cpp)
add_library (live555helper STATIC ${LIVEHELPERSOURCE})	
target_include_directories(live555helper PUBLIC live/groupsock/include live/liveMedia/include live/UsageEnvironment/include live/BasicUsageEnvironment/include live555helper/inc)
target_link_libraries (${PROJECT_NAME} live555helper) 
add_definitions(-DHAVE_LIVE555)

# compiler specific
if (MSVC)
	# live555
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	target_link_libraries (${PROJECT_NAME} ws2_32)
	# getopt
    add_library (getopt getopt/getopt.c)
	target_include_directories(getopt PUBLIC getopt) 
	target_link_libraries (${PROJECT_NAME} getopt)
	# webrtc
	add_definitions(-DWEBRTC_WIN -EHsc -DNOMINMAX -D__PRETTY_FUNCTION__=__FUNCTION__)
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

#cpack
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install (DIRECTORY html DESTINATION .)
SET(CPACK_GENERATOR "TGZ")
SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
SET(CPACK_PACKAGE_CONTACT "michel.promonet@free.fr") 
if(PROJECTVERSION)
    SET(CPACK_PACKAGE_VERSION "${PROJECTVERSION}")
endif()
INCLUDE(CPack)
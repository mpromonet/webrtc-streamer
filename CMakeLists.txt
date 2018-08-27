cmake_minimum_required (VERSION 3.0.0)
 
project (webrtc-streamer)
set(EXECUTABLE_OUTPUT_PATH ./${CMAKE_BUILD_TYPE})
 
set (SOURCE civetweb/src/civetweb.c civetweb/src/CivetServer.cpp src/HttpServerRequestHandler.cpp src/PeerConnectionManager.cpp src/main.cpp)

set (CMAKE_BUILD_TYPE Release) 
set (CMAKE_C_FLAGS "-DNDEBUG=1 -DWEBRTC_WIN -DHAVE_JPEG /EHsc -DNOMINMAX -DWEBRTC_WIN -DVERSION='ABC'")
set (CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
#set (CMAKE_EXE_LINKER_FLAGS "/MT")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

include_directories(civetweb/include inc ../webrtc/src ../webrtc/src/third_party/abseil-cpp ../webrtc/src/third_party/jsoncpp/source/include) 

set (OBJS ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_value.obj
            ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_reader.obj
            ../webrtc/src/out/Release/obj/third_party/jsoncpp/jsoncpp/json_writer.obj
            ../webrtc/src/out/Release/obj/rtc_base/rtc_json/json.obj)

add_executable (${PROJECT_NAME} ${SOURCE} ${OBJS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ../webrtc/src/out/Release/obj/webrtc secur32 dmoguids wmcodecdspuuid strmiids msdmo winmm)

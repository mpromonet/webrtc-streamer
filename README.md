WebRTC-streamer
===============

This is a try to stream video sources through WebRTC using simple mechanism.

It embeds a HTTP server that implements API and serve a simple HTML page that use them through AJAX.

The WebRTC signaling is implemented throught HTTP requests:

 - /api/call   : send offer and get answer
 - /api/hangup : close a call

 - /api/addIceCandidate : add a candidate
 - /api/getIceCandidate : get the list of candidates


Dependencies :
-------------
It is based on :
 * [WebRTC Native Code Package](http://www.webrtc.org)
 * [civetweb HTTP server](https://github.com/civetweb/civetweb)
 * [h264bitstream](https://github.com/aizvorski/h264bitstream)

Build
===============

1- Install Toolchain (https://chromium.googlesource.com/chromium/src/+/lkgr/docs/linux_build_instructions_prerequisites.md)

2- Install Depot Tools (https://dev.chromium.org/developers/how-tos/install-depot-tools)

Then:

Build WebRTC with H264 support
-------
	mkdir ../webrtc
	pushd ../webrtc
	fetch webrtc
	gn gen out/Release --args='is_debug=false use_custom_libcxx=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_include_pulse_audio=false use_sysroot=false is_clang=false treat_warnings_as_errors=false'
	ninja -C out/Release 
	popd



Build WebRTC Streamer
-------
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug> SYSROOT=<path to WebRTC>/src/build/linux/debian_stretch_amd64-sysroot

where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source (default is ../webrtc)
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries (default is Release)
 - $SYSROOT should point to sysroot used to build WebRTC (default is /)

Usage
===============
	./webrtc-streamer [-H http port] [-S[embeded stun address]] -[v[v]]  [url1]...[urln]
	./webrtc-streamer [-H http port] [-s[external stun address]] -[v[v]] [url1]...[urln]
	./webrtc-streamer -V
         	-H [hostname:]port : HTTP server binding (default 0.0.0.0:8000)
         	-S[stun_address]   : start embeded STUN server bind to address (default 0.0.0.0:3478)
         	-s[stun_address]   : use an external STUN server (default stun.l.google.com:19302)
         	-t[username:password@]turn_address : use an external TURN relay server (default disabled)
        	-a[audio layer]    : spefify audio capture layer to use (default:3)
         	[url]              : url to register in the source list
        	-v[v[v]]           : verbosity
        	-V                 : print version

Arguments of '-H' is forwarded to option 'listening_ports' of civetweb, then it is possible to use the civetweb syntax like '-H8000,9000' or '-H8080r,8443'.


[Live Demo]

You can access to the WebRTC stream coming from an RTSP url using [webrtcstreamer.html](html/webrtcstreamer.html) page with the VNC url as argument, something like:

   http://localhost:8000/webrtcstreamer.html?vnc://217.17.220.110


Docker image
===============
You can start the application using the docker image :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer

You can expose V4L2 devices from your host using :

        docker run --device=/dev/video0 -p 8000:8000 -it mpromonet/webrtc-streamer

The container entry point is the webrtc-streamer application, then you can :

* get the help using :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer -h

* run the container registering a VNC url using :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer -u vnc://pi2.local:8554





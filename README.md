webrtc-streamer
-------------

This is a try to stream a video source through WebRTC.  
It embeds a HTTP server that implements API and serve a simple HTML page that use them through AJAX.   
The WebRTC signaling is implemented throught HTTP requests:

 - /call   : send offer and get answer
 - /hangup : close a call

 - /addIceCandidate : add a candidate
 - /getIceCandidate : get the list of candidates

An other HTTP API /getDeviceList give available sources.
A video source could be :
 - V4L2 devices detected by WebRTC capture factory
 - RTSP url that give access to an H264 video stream (need live555)

It is based on WebRTC Native Code Package [http://www.webrtc.org]

License
-------
Domain Public

Dependencies
------------
 - WebRTC 
 - live555 for RTSP connection

Build WebRTC
-------
	mkdir ../webrtc
	pushd ../webrtc
	fetch webrtc
	gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false' 
	ninja -C out/Release
	popd


Build
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug>
	
where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source 
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries

Usage
-----
	./webrtc-server__Release [-H http port] [-S embeded stun address] -[v[v]]  [url1]...[urln]
	./webrtc-server__Release [-H http port] [-s externel stun address] -[v[v]] [url1]...[urln]
        	-v[v[v]]           : verbosity
         	-H [hostname:]port : HTTP server binding (default 0.0.0.0:8000)
         	-S stun_address    : start embeded STUN server bind to address (default 127.0.0.1:3478)
         	-s[stun_address]   : use an external STUN server (default stun.l.google.com:19302)
         	[url]              : url to register in the source list


Next you can connect to the interface using a Web browser with the URL of the HTTP server.


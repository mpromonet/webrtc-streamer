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
 - RTSP url that give access to an H264 video stream

It is based on WebRTC Native Code Package [http://www.webrtc.org]

License
-------
Domain Public

Dependencies
------------
 - WebRTC 
 
Build
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug>
	
where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source 
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries

Usage
-----
	./webrtc-streamer [-v[v]] [-H HTTP binding] [-S STUN binding]
		 -v[v[v]] : verbosity
		 -H hostname:port  : HTTP binding (default 0.0.0.0:8000)
		 -S hostname:port  : use embeded STUN server (default 127.0.0.1:3478)
		 -s[stun_address]  : use an external STUN server (default stun.l.google.com:19302)


Next you can connect to the interface using a Web browser with the URL of the HTTP server.


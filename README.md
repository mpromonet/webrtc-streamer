webrtc-server
-------------

This is a try to stream a V4L2 linux device through webRTC.

It is based on :
 * WebRTC Native Code Package [http://www.webrtc.org/reference/getting-started]
 * mongoose [https://github.com/cesanta/mongoose]

Dependencies
------------
 - webrtc
 
Build
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug>
	
Usage
-----
	./webrtc-server [-v[v]] [-P http port]
		 -v[v[v]] : verbosity
		 -P port  : HTTP port (default 8080)


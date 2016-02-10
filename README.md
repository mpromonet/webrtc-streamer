webrtc-server
-------------

This is a try to stream a V4L2 linux capture device through WebRTC.

It is based on :
 * WebRTC Native Code Package [http://www.webrtc.org/reference/getting-started]

License
-------
Domain Public
 
Build
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug>
	
Usage
-----
	./webrtc-server [-v[v]] [-P HTTP binding] [-S STUN binding]
		 -v[v[v]] : verbosity
		 -H hostname:port  : HTTP binding (default 0.0.0.0:8000)
		 -S hostname:port  : STUN binding (default 127.0.0.1:3478)


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
	make WEBRTCROOT=<path to WebRTC>
	
Usage
-----
	./webrtc-server [-P http port] [device]
		 -v[v[v]] : verbosity
		 -P port  : HTTP port (default 8080)

Limitation
----------
Each peer connection open the capture device, so video4Linux device serve only one peer.

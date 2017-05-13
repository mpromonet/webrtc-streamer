[![Build status](https://travis-ci.org/mpromonet/webrtc-streamer.png)](https://travis-ci.org/mpromonet/webrtc-streamer)
[![Docker Pulls](https://img.shields.io/docker/pulls/mpromonet/webrtc-streamer.svg)](https://hub.docker.com/r/mpromonet/webrtc-streamer/)

[![Heroku](https://heroku-badge.herokuapp.com/?app=webrtc-streamer)](https://webrtc-streamer.herokuapp.com/)

WebRTC-streamer
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

Dependencies :
--------
It is based on :
 * WebRTC Native Code Package [http://www.webrtc.org] 
 * civetweb HTTP server [https://github.com/civetweb/civetweb]

Build WebRTC with H264 support
-------
	mkdir ../webrtc
	pushd ../webrtc
	fetch webrtc
	gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false'
	ninja -C out/Release
	popd


Build live555 to enable RTSP support(optional)
-------
	wget http://www.live555.com/liveMedia/public/live555-latest.tar.gz -O - | tar xzf -
	pushd live
	./genMakefiles linux
	sudo make install
	popd

Build
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug> PREFIX=/usr/local
	
where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source 
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries
and where PREFIX point to live555 installation (default is /usr/local)

Usage
-----
	./webrtc-server__Release [-H http port] [-S embeded stun address] -[v[v]]  [url1]...[urln]
	./webrtc-server__Release [-H http port] [-s externel stun address] -[v[v]] [url1]...[urln]
        	-v[v[v]]           : verbosity
         	-H [hostname:]port : HTTP server binding (default 0.0.0.0:8000)
         	-S stun_address    : start embeded STUN server bind to address (default 127.0.0.1:3478)
         	-s[stun_address]   : use an external STUN server (default stun.l.google.com:19302)
         	[url]              : url to register in the source list


Example
-----
	webrtc-server__Release rtsp://217.17.220.110/axis-media/media.amp \
				rtsp://85.255.175.241/h264 \
				rtsp://85.255.175.244/h264 \
				rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov


![Screenshot](snapshot.png)

[Live Demo](https://webrtc-streamer.herokuapp.com/)

You can access to the WebRTC stream coming from an RTSP url using [webrtcstream.html](html/webrtcstream.html) page with the RTSP url as argument, something like:

     https://webrtc-streamer.herokuapp.com/webrtcstream.html?rtsp://217.17.220.110/axis-media/media.amp

Embed in a HTML page:
------------------------
Instead of using the internal HTTP server, it is easy to display a WebRTC stream in a HTML page served by an external HTTP server. The URL of the webrtc-streamer to use should be given creating the WebRtcStreamer instance :

	var webRtcServer      = new WebRtcStreamer(<video tag>, <url of webrtc-streamer>);

A short sample using webrtc-streamer running locally on port 8000 :

	<html>
	<head>
	<script src="ajax.js" ></script>
	<script src="webrtcstreamer.js" ></script>
	<script>        
	    var webRtcServer      = new WebRtcStreamer("video",location.protocol+"//"+window.location.hostname+":8000");
	    window.onload         = function() { webRtcServer.connect(location.search.slice(1)) }
	    window.onbeforeunload = function() { webRtcServer.disconnect() }
	</script>
	</head>
	<body> 
	    <video id="video" />
	</body>
	</html>
	



 



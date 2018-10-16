[![TravisCI](https://travis-ci.org/mpromonet/webrtc-streamer.png)](https://travis-ci.org/mpromonet/webrtc-streamer)
[![CircleCI](https://circleci.com/gh/mpromonet/webrtc-streamer.svg?style=shield)](https://circleci.com/gh/mpromonet/webrtc-streamer)
[![Appveyor](https://ci.appveyor.com/api/projects/status/github/mpromonet/webrtc-streamer?branch=master&svg=true)](https://ci.appveyor.com/project/mpromonet/webrtc-streamer/build/artifacts)

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/c209c81a15854964a08df5c300f56804)](https://www.codacy.com/app/michelpromonet_2643/webrtc-streamer?utm_source=github.com&utm_medium=referral&utm_content=mpromonet/webrtc-streamer&utm_campaign=badger)

[![Release](https://img.shields.io/github/release/mpromonet/webrtc-streamer.svg)](https://github.com/mpromonet/webrtc-streamer/releases/latest)
[![Download](https://img.shields.io/github/downloads/mpromonet/webrtc-streamer/total.svg)](https://github.com/mpromonet/webrtc-streamer/releases/latest)
[![Docker Pulls](https://img.shields.io/docker/pulls/mpromonet/webrtc-streamer.svg)](https://hub.docker.com/r/mpromonet/webrtc-streamer/)

[![Heroku](https://heroku-badge.herokuapp.com/?app=webrtc-streamer)](https://webrtc-streamer.herokuapp.com/)
[![Heroku](https://heroku-badge.herokuapp.com/?app=rtsp2webrtc)](https://rtsp2webrtc.herokuapp.com/)

WebRTC-streamer
===============

This is a try to stream video sources through WebRTC using simple mechanism.  

It embeds a HTTP server that implements API and serves a simple HTML page that use them through AJAX.   

The WebRTC signaling is implemented through HTTP requests:

 - /api/call   : send offer and get answer
 - /api/hangup : close a call

 - /api/addIceCandidate : add a candidate
 - /api/getIceCandidate : get the list of candidates

The list of HTTP API is available using /api/help.

Nowadays there are 3 builds on [CircleCI](https://circleci.com/gh/mpromonet/webrtc-streamer) :
 * for x86_64 on Ubuntu Xenial
 * for armv7 crosscompiling with gcc-linaro-arm-linux-gnueabihf-raspbian-x64 (this build is running on Raspberry Pi2 and NanoPi NEO)
 * for armv6+vfp crosscompiling with gcc-linaro-arm-linux-gnueabihf-raspbian-x64 (this build is running on Raspberry PiB and should run on a Raspberry Zero)
 
and a build on [Appveyor](https://ci.appveyor.com/project/mpromonet/webrtc-streamer/build/artifacts) :
* for Windows using Visual 2017 64bits

The webrtc stream name could be :
 * an alias defined using -n argument then the corresponding -u argument will be used to create the capturer
 * an "rtsp://" url that will be openned using an RTSP capturer based on live555
 * an "screen://" url that will be openned by webrtc::DesktopCapturer::CreateScreenCapturer 
 * an "window://" url that will be openned by webrtc::DesktopCapturer::CreateWindowCapturer 
 * a V4L2 capture device name

Dependencies :
-------------
It is based on :
 * [WebRTC Native Code Package](http://www.webrtc.org)
 * [civetweb HTTP server](https://github.com/civetweb/civetweb)
 * [live555](http://www.live555.com/liveMedia) for RTSP cpaturer

Build
===============
Install the Chromium depot tools (for WebRTC).
-------
	pushd ..
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
	export PATH=$PATH:`realpath depot_tools`
	popd

Build WebRTC with H264 support
-------
	mkdir ../webrtc
	pushd ../webrtc
	fetch webrtc
	cd ./src
	gn gen out/Release --args='is_debug=false use_custom_libcxx=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_include_pulse_audio=false use_sysroot=false is_clang=false treat_warnings_as_errors=false'
	ninja -C out/Release 
	popd


Build live555 to enable RTSP support (optional)
-------
	make live555

Build WebRTC Streamer
------- 
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug>
	
where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source (default is $(pwd)/../webrtc) 
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries (default is Release)

Build using cmake (experimental)
-------

       cmake .
       make

Build html preview (optional)
-------
	git submodule update --init html
	make install

With this, you can access `http://HOST:PORT/webrtcstreamer.html?:SourceNameOrURL` to preview in browser.

Usage
===============
	./webrtc-streamer [-H http port] [-S[embeded stun address]] -[v[v]]  [url1]...[urln]
	./webrtc-streamer [-H http port] [-s[external stun address]] -[v[v]] [url1]...[urln]
	./webrtc-streamer -V
        	-v[v[v]]           : verbosity
        	-V                 : print version
         	-H [hostname:]port : HTTP server binding (default 0.0.0.0:8000)

         	-S[stun_address]   : start embeded STUN server bind to address (default 0.0.0.0:3478)
         	-s[stun_address]   : use an external STUN server (default stun.l.google.com:19302)
         	-t[username:password@]turn_address : use an external TURN relay server (default disabled)		

        	-a[audio layer]    : spefify audio capture layer to use (default:3)		

		-n name -u url     : register a name for an url
         	[url]              : url to register in the source list

Arguments of '-H' are forwarded to option [listening_ports](https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#listening_ports-8080) of civetweb, then it is possible to use the civetweb syntax like '-H8000,9000' or '-H8080r,8443s'.

Example
-----
	webrtc-streamer rtsp://217.17.220.110/axis-media/media.amp \
				rtsp://85.255.175.241/h264 \
				rtsp://85.255.175.244/h264 \
				rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov


[![Screenshot](images/snapshot.png)](https://rtsp2webrtc.herokuapp.com/)

[Live Demo](https://rtsp2webrtc.herokuapp.com/)

We can access to the WebRTC stream using [webrtcstreamer.html](https://github.com/mpromonet/webrtc-streamer-html/blob/master/webrtcstreamer.html) for instance :

 * https://rtsp2webrtc.herokuapp.com/webrtcstreamer.html?rtsp://217.17.220.110/axis-media/media.amp
 * https://rtsp2webrtc.herokuapp.com/webrtcstreamer.html?Bunny

An example displaying grid of WebRTC Streams is available using option "layout=<lines>x<columns>"
[![Screenshot](images/layout2x4.png)](https://rtsp2webrtc.herokuapp.com/?layout=2x4)

Embed in a HTML page:
===============
Instead of using the internal HTTP server, it is easy to display a WebRTC stream in a HTML page served by another HTTP server. The URL of the webrtc-streamer to use should be given creating the [WebRtcStreamer](http://htmlpreview.github.io/?https://github.com/mpromonet/webrtc-streamer-html/blob/master/jsdoc/WebRtcStreamer.html) instance :

	var webRtcServer      = new WebRtcStreamer(<video tag>, <webrtc-streamer url>);

A short sample HTML page using webrtc-streamer running locally on port 8000 :

	<html>
	<head>
	<script src="lib/request.min.js" ></script>
	<script src="webrtcstreamer.js" ></script>
	<script>        
	    var webRtcServer      = new WebRtcStreamer("video",location.protocol+"//"+window.location.hostname+":8000");
	    window.onload         = function() { webRtcServer.connect("rtsp://pi2.local:8554/unicast") }
	    window.onbeforeunload = function() { webRtcServer.disconnect() }
	</script>
	</head>
	<body> 
	    <video id="video" />
	</body>
	</html>

Connect to Janus Gateway Video Room
===============
A simple way to publish WebRTC stream to a [Janus Gateway](https://janus.conf.meetecho.com) Video Room is to use the [JanusVideoRoom](http://htmlpreview.github.io/?https://github.com/mpromonet/webrtc-streamer-html/blob/master/jsdoc/JanusVideoRoom.html) interface

        var janus = new JanusVideoRoom(<janus url>, <webrtc-streamer url>)

A short sample to publish WebRTC streams to Janus Video Room could be :

	<html>
	<head>
	<script src="lib/request.min.js" ></script>
	<script src="janusvideoroom.js" ></script>
	<script>        
		var janus = new JanusVideoRoom("https://janus.conf.meetecho.com/janus", null);
		janus.join(1234, "rtsp://pi2.local:8554/unicast","pi2");
		janus.join(1234, "rtsp://217.17.220.110/axis-media/media.amp","media");	    
	</script>
	</head>
	</html>

[![Screenshot](images/janusvideoroom.png)](https://rtsp2webrtc.herokuapp.com/janusvideoroom.html)

[Live Demo](https://rtsp2webrtc.herokuapp.com/janusvideoroom.html)

This way the communication between [Janus API](https://janus.conf.meetecho.com/docs/JS.html) and [WebRTC Streamer API](https://rtsp2webrtc.herokuapp.com/help) is implemented in Javascript running in browser.

The same logic could be implemented in NodeJS using the same JS API :

	global.request = require('then-request');
	var JanusVideoRoom = require('./html/janusvideoroom.js'); 
	var janus = new JanusVideoRoom("http://192.168.0.15:8088/janus", "http://192.168.0.15:8000")
	janus.join(1234,"mmal service 16.1","video")

Connect to Jitsi 
===============
A simple way to publish WebRTC stream to a [Jitsi](https://meet.jit.si) Video Room is to use the [XMPPVideoRoom](http://htmlpreview.github.io/?https://github.com/mpromonet/webrtc-streamer-html/blob/master/jsdoc/XMPPVideoRoom.html) interface

        var xmpp = new XMPPVideoRoom(<xmpp server url>, <webrtc-streamer url>)

A short sample to publish WebRTC streams to a Jitsi Video Room could be :

	<html>
	<head>
	<script src="libs/strophe.min.js" ></script>
	<script src="libs/strophe.muc.min.js" ></script>
	<script src="libs/strophe.disco.min.js" ></script>
	<script src="libs/strophe.jingle.sdp.js"></script>
	<script src="libs/jquery-1.12.4.min.js"></script>
	<script src="libs/request.min.js" ></script>
	<script src="xmppvideoroom.js" ></script>
	<script>        
		var xmpp = new XMPPVideoRoom("meet.jit.si", null);
		xmpp.join("testroom", "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov","Bunny");	    
	</script>
	</head>
	</html>

[Live Demo](https://rtsp2webrtc.herokuapp.com/xmppvideoroom.html)

Docker image
===============
You can start the application using the docker image :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer

You can expose V4L2 devices from your host using :

        docker run --device=/dev/video0 -p 8000:8000 -it mpromonet/webrtc-streamer

The container entry point is the webrtc-streamer application, then you can :

* get the help using :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer -h

* run the container registering a RTSP url using :

        docker run -p 8000:8000 -it mpromonet/webrtc-streamer -n raspicam -u rtsp://pi2.local:8554/unicast





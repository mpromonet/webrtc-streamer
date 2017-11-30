#!/usr/bin/env node
/*
* NodeJS example to send a webrtc-streamer stream to janus-gateway
*/

// decode arguments
if (process.argv.length <= 2) {
    console.log("Usage: " + __filename + " <webrtc-streamer url> <videourl>");
    process.exit(-1);
}
var webrtcstreamerurl = process.argv[2];
console.log('webrtcstreamerurl: ' + webrtcstreamerurl);
var videourl = process.argv[3];
console.log('videourl: ' + videourl);

// get configuration from webrtc-streamer using janusvideoroom.json
var request = require('then-request');
var JanusVideoRoom = require('./html/janusvideoroom.js'); 

request( 'GET',  webrtcstreamerurl + "/janusvideoroom.json" ).done(
	function (response) { 
		if (response.statusCode === 200) {
			console.log("HTTP answer:"+ response.body);
			eval("{" + response.body + "}");
			console.log(JSON.stringify(request));
			var janus = new JanusVideoRoom(janusRoomConfig.url, webrtcstreamerurl, request)
			janus.join(janusRoomConfig.roomId,videourl,"video")
		} else {
			console.log("HTTP code:"+ response.statusCode);
		}
	}
)


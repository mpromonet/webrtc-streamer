#!/usr/bin/env node
/*
* NodeJS example to send a webrtc-streamer stream to janus-gateway
*/

// callback to send http requests
function send(method,headers,data,onSuccess,onFailure,scope) {

	console.log("HTTP call "+ method);
	var request = require('request');
	var verb = 'GET';
	if (data) {
		verb = 'POST';
		data = JSON.stringify(data);
	}	
	request({
			method: verb,
			uri: method,
			body: data,
			headers: headers
		},
		function (error, response, body) { 
			if ( !error && (response.statusCode === 200) && onSuccess ) {
				onSuccess.call(scope,JSON.parse(body));
			}
			else if (onFailure) {
				onFailure.call(scope,error);
			}
		}
	)
}

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
var request = require('request');
request( { uri: webrtcstreamerurl + "/janusvideoroom.json" },
	function (error, response, body) { 
		if (!error && (response.statusCode === 200) ) {
			console.log("HTTP answer:"+ body);
			eval(body);
			var JanusVideoRoom = require('./html/janusvideoroom.js'); 
			var janus = new JanusVideoRoom(janusRoomConfig.url, null, webrtcstreamerurl, send)
			janus.join(janusRoomConfig.roomId,videourl,"video")
		} else {
			console.log("HTTP code:"+ error);
		}
	}
)


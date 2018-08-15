#!/usr/bin/env node
/*
* NodeJS example to send a webrtc-streamer stream to jitsi
*/
process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";
// decode arguments
if (process.argv.length <= 2) {
    console.log("Usage: " + __filename + " <webrtc-streamer url> <videourl>");
    process.exit(-1);
}
var webrtcstreamerurl = process.argv[2];
console.log("webrtcstreamerurl: " + webrtcstreamerurl);
var videourl = process.argv[3];
console.log("videourl: " + videourl);

// get configuration from webrtc-streamer using janusvideoroom.json
request = require("then-request");
strophe = require("node-strophe").Strophe;
Strophe = strophe.Strophe;

var XMPPVideoRoom = require("./html/xmppvideoroom.js"); 

request( "GET",  webrtcstreamerurl + "/xmppvideoroom.json" ).done(
	function (response) { 
		if (response.statusCode === 200) {
			console.log("HTTP answer:"+ response.body);
			eval("{" + response.body + "}");
			
			var xmpp = new XMPPVideoRoom(xmppRoomConfig.url, webrtcstreamerurl);
			xmpp.join(xmppRoomConfig.roomId,videourl,"video");
		} else {
			console.log("HTTP code:"+ response.statusCode);
		}
	}
);


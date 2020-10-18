#!/usr/bin/env node
/*
* NodeJS example to send a webrtc-streamer stream to janus-gateway
*/

// decode arguments
if (process.argv.length < 4) {
    console.log("Usage: " + __filename + " <webrtc-streamer url> <videourl> <janus url> <janus room>");
    process.exit(-1);
}
var webrtcstreamerurl = process.argv[2];
console.log("webrtcstreamerurl: " + webrtcstreamerurl);
var videourl = process.argv[3];
console.log("videourl: " + videourl);
var janusRoomUrl = process.argv[4];
console.log("janusRoomUrl: " + janusRoomUrl);
var roomId = 1234
if (process.argv.length >= 5) {
	roomId = process.argv[5];
}
console.log("roomId: " + roomId);

global.fetch = require("node-fetch");
var JanusVideoRoom = require("./html/janusvideoroom.js"); 
var janus = new JanusVideoRoom(janusRoomUrl, webrtcstreamerurl);

janus.join(roomId,videourl,"video");



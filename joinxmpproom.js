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

var jsdom = require("jsdom");
const { JSDOM } = jsdom;
const { window } = new JSDOM();
const { document } = (new JSDOM('')).window;
global.window = window;
global.document = document;
global.DOMParser = window.DOMParser;
global.XMLHttpRequest = window.XMLHttpRequest;

global.$ = jQuery = require('jquery')(global.window);

var strophe = require("strophe.js");
global.Strophe = strophe.Strophe;
global.$iq = strophe.$iq;
//global.Strophe.log = console.log;

require("strophejs-plugin-disco");
require("strophejs-plugin-caps"); 
require("strophejs-plugin-muc"); 

global.SDP = require("strophe.jingle/strophe.jingle.sdp.js");
request = require("then-request");
var XMPPVideoRoom = require("./html/xmppvideoroom.js"); 

var a = new SDP("");

// get configuration from webrtc-streamer using janusvideoroom.json
request( "GET",  webrtcstreamerurl + "/xmppvideoroom.json" ).done(
	function (response) { 
		if (response.statusCode === 200) {
			console.log("HTTP answer:"+ response.body);
			eval("{" + response.body + "}");
			
			var xmpp = new XMPPVideoRoom(xmppRoomConfig.url, webrtcstreamerurl);
			xmpp.join(xmppRoomConfig.roomId,videourl,"user");
		} else {
			console.log("HTTP code:"+ response.statusCode);
		}
	}
);


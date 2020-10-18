#!/usr/bin/env node
/*
* NodeJS example to send a webrtc-streamer stream to jitsi
*/
process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";
// decode arguments
if (process.argv.length <= 4) {
    console.log("Usage: " + __filename + " <webrtc-streamer url> <videourl> <xmpp url> <xmpp room>");
    process.exit(-1);
}
var webrtcstreamerurl = process.argv[2];
console.log("webrtcstreamerurl: " + webrtcstreamerurl);
var videourl = process.argv[3];
console.log("videourl: " + videourl);
var xmppRoomUrl = process.argv[4];
console.log("xmppRoomUrl: " + xmppRoomUrl);
var xmppRoomId = "testroom"
if (process.argv.length >= 5) {
	xmppRoomId = process.argv[5];
}
console.log("xmppRoomId: " + xmppRoomId);

var jsdom = require("jsdom");
const { JSDOM } = jsdom;
const { window } = new JSDOM("");

global.jquery = require("jquery")(window);
global.$ = (selector,context) => {return new jquery.fn.init(selector,context); };

global.window = window;
global.document = window.document;
global.DOMParser = window.document.DOMParser;
global.XMLHttpRequest = window.XMLHttpRequest;

var strophe = require("strophe.js");
global.Strophe = strophe.Strophe;
global.$iq = strophe.$iq;
//global.Strophe.log = console.log;

require("strophejs-plugin-disco");
require("strophejs-plugin-muc"); 

global.SDP = require("strophe.jingle/strophe.jingle.sdp.js");

global.fetch = require("node-fetch");
var XMPPVideoRoom = require("./html/xmppvideoroom.js"); 

			
var xmpp = new XMPPVideoRoom(xmppRoomUrl, webrtcstreamerurl);
var username = "user"+Math.random().toString(36).slice(2);
console.log("join " + xmppRoomId + "/" + username);

xmpp.join(xmppRoomId,videourl,username);


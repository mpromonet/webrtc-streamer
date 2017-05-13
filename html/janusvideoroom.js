function write(text) {
	console.log(text);
}

// ------------------------------------------
// Constructor
// ------------------------------------------
function JanusVideoRoom (janusUrl, janusRoomId, callback) {	
	this.janusUrl    = janusUrl;
	this.janusRoomId = janusRoomId;	
	this.callback    = callback || function() {};	
}
	
// ------------------------------------------
// Ask to connect an URL to a Janus Video Room user
// ------------------------------------------
JanusVideoRoom.prototype.connect = function(url,name) {
	// create a session
	var createReq = {janus: "create", transaction: Math.random().toString() }
	send(this.janusUrl, null, createReq, function(dataJson) { this.onCreateSession(dataJson,url,name) }, this.onError, this);		
}

// ------------------------------------------
// Janus callback for Session Creation
// ------------------------------------------
JanusVideoRoom.prototype.onCreateSession = function(dataJson,url,name) {
	var sessionId = dataJson.data.id;
	write("onCreateSession sessionId:" + sessionId);
	
	// attach to video room plugin
	var attach = { "janus": "attach", "plugin": "janus.plugin.videoroom", "transaction": Math.random().toString() };			
	send(this.janusUrl + "/" + sessionId, null, attach, function(dataJson) { this.onPluginsAttached(dataJson,url,name,sessionId) }, this.onError, this );
}
	
// ------------------------------------------
// Janus callback for Video Room Plugins Connection
// ------------------------------------------
JanusVideoRoom.prototype.onPluginsAttached = function(dataJson,url,name,sessionId) {
	var pluginid = dataJson.data.id;
	write("onPluginsAttached pluginid:" + pluginid)	
	
	this.callback(name, "joining room");

	var join = {"janus":"message","body":{"request":"join","room":this.janusRoomId,"ptype":"publisher","display":name},"transaction":Math.random().toString()}
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, join, function(dataJson) { this.onJoinRoom(dataJson,name,url,sessionId,pluginid) }, this.onError, this );		
}

// ------------------------------------------
// Janus callback for Video Room Joined
// ------------------------------------------
JanusVideoRoom.prototype.onJoinRoom = function(dataJson,name,url,sessionId,pluginid) {
	write("onJoinRoom:" + JSON.stringify(dataJson))

	var answer = sendSync(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1");
	write("onJoinRoom evt:" + JSON.stringify(answer))
	if (answer.plugindata.data.videoroom === "joined") {
		this.callback(name, "room joined");
		
		var peerid = Math.random().toString();
		send("/createOffer?peerid="+ peerid+"&url="+encodeURIComponent(url), null, null, function(dataJson) { this.onCreateOffer(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this); 
	} else {
		this.callback(name, "joining room failed");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Offer 
// ------------------------------------------
JanusVideoRoom.prototype.onCreateOffer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onCreateOffer:" + JSON.stringify(dataJson))
	
	this.callback(name, "publishing stream");
	
	var msg = { "janus": "message", "body": {"request": "publish", "video": true, "audio": false}, "jsep": dataJson, "transaction": Math.random().toString() };		
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg,  function(dataJson) { this.onPublishStream(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this);
}

// ------------------------------------------
// Janus callback for WebRTC stream is published
// ------------------------------------------
JanusVideoRoom.prototype.onPublishStream = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onPublishStream:" + JSON.stringify(dataJson))	

	var answer = sendSync(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1");
	write("onPublishStream evt:" + JSON.stringify(answer))
	
	if (answer.jsep) {
		send("/setAnswer?peerid="+ peerid, null, answer.jsep, function(dataJson) { this.onSetAnswer(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this); 						
	} else {
		this.callback(name, "publishing failed (no SDP)");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Answer 
// ------------------------------------------
JanusVideoRoom.prototype.onSetAnswer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onSetAnswer:" + JSON.stringify(dataJson))	
	
	send("/getIceCandidate?peerid="+peerid, null, null, function(dataJson) { this.onReceiveCandidate(dataJson,name,sessionId,pluginid) }, this.onError, this);		
}

// ------------------------------------------
// WebRTC streamer callback for ICE candidate 
// ------------------------------------------
JanusVideoRoom.prototype.onReceiveCandidate = function(dataJson,name,sessionId,pluginid) {
	write("onReceiveCandidate answer:" + JSON.stringify(dataJson))	
	
	for (var i=0; i<dataJson.length; i++) {
		var candidate = new RTCIceCandidate(dataJson[i]);

		// send ICE candidate to Janus
		var msg = { "janus": "trickle", "candidate": candidate, "transaction": Math.random().toString()  };
		sendSync(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg);		
	}
	
	// start long polling
	this.longpoll(null, name, sessionId);	
}

// ------------------------------------------
// Janus callback for keepAlive Session
// ------------------------------------------
JanusVideoRoom.prototype.keepAlive = function(sessionId) {
	var msg = { "janus": "keepalive", "session_id": sessionId, "transaction": Math.random().toString()  };
	var answer = sendSync(this.janusUrl + "/" + sessionId, null, msg);
	write("keepAlive :" + JSON.stringify(answer))
}

// ------------------------------------------
// Janus callback for Long Polling
// ------------------------------------------
JanusVideoRoom.prototype.longpoll = function(dataJson, name, sessionId) {
	if (dataJson) {
		write("poll evt:" + JSON.stringify(dataJson))		
	
		if (dataJson.janus == "webrtcup") {
			// notify connection
			this.callback(name, "connected");
			
			// start keep alive
			var bind = this;
			window.setInterval( function() { bind.keepAlive(sessionId); }, 10000);	
		}
	}
	
	send(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1", null, null, function(dataJson) { this.longpoll(dataJson, name, sessionId) }, function(dataJson) { this.longpoll(dataJson, name, sessionId) }, this);
}

	







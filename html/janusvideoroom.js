function write(text) {
	console.log(text);
}

/** 
 * Interface with Janus Gateway Video Room and WebRTC-streamer API
 * @constructor
 * @param {string} janusUrl - url of Janus Gateway
 * @param {function} callback - callback to trig when state changes
 * @param {string} srvurl - url of WebRTC-streamer
*/
function JanusVideoRoom (janusUrl, callback, srvurl) {	
	this.janusUrl    = janusUrl;
	this.callback    = callback || function() {};	
	this.srvurl      = srvurl || location.protocol+"//"+window.location.hostname+":"+window.location.port;
	this.connection  = [];
}
	
/** 
* Ask to publish a stream from WebRTC-streamer in a Janus Video Room user
 * @param {string} janusroomid - id of the Janus Video Room to join
 * @param {string} url - WebRTC stream to publish
 * @param {string} name - name in  Janus Video Room
*/
JanusVideoRoom.prototype.join = function(janusroomid, url, name) {
	// create a session
	var createReq = {janus: "create", transaction: Math.random().toString() }
	send(this.janusUrl, null, createReq, function(dataJson) { this.onCreateSession(dataJson, janusroomid, url, name) }, this.onError, this);		
}

/**
* Ask to unpublish a stream from WebRTC-streamer in a Janus Video Room user
 * @param {string} janusroomid - id of the Janus Video Room to join
 * @param {string} url - WebRTC stream to publish
 * @param {string} name - name in  Janus Video Room
*/
JanusVideoRoom.prototype.leave = function(janusroomid, url, name) {
	var connection = this.connection[janusroomid + "_" + url + "_" + name];
	if (connection) {
		var sessionId = connection.sessionId;
		var pluginid  = connection.pluginId;
		
		var msg = { "janus": "message", "body": {"request": "unpublish"}, "transaction": Math.random().toString() };		
		send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg,  null, this.onError, this);
	}
}


// ------------------------------------------
// Janus callback for Session Creation
// ------------------------------------------
JanusVideoRoom.prototype.onCreateSession = function(dataJson, janusroomid, url, name) {
	var sessionId = dataJson.data.id;
	write("onCreateSession sessionId:" + sessionId);
	
	// attach to video room plugin
	var attach = { "janus": "attach", "plugin": "janus.plugin.videoroom", "transaction": Math.random().toString() };			
	send(this.janusUrl + "/" + sessionId, null, attach, function(dataJson) { this.onPluginsAttached(dataJson, janusroomid, url, name, sessionId) }, this.onError, this );
}
	
// ------------------------------------------
// Janus callback for Video Room Plugins Connection
// ------------------------------------------
JanusVideoRoom.prototype.onPluginsAttached = function(dataJson, janusroomid, url, name, sessionId) {
	var pluginid = dataJson.data.id;
	write("onPluginsAttached pluginid:" + pluginid)	
	
	this.callback(name, "joining");

	var join = {"janus":"message","body":{"request":"join","room":janusroomid,"ptype":"publisher","display":name},"transaction":Math.random().toString()}
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, join, function(dataJson) { this.onJoinRoom(dataJson,janusroomid,name,url,sessionId,pluginid) }, this.onError, this );		
}

// ------------------------------------------
// Janus callback for Video Room Joined
// ------------------------------------------
JanusVideoRoom.prototype.onJoinRoom = function(dataJson,janusroomid,name,url,sessionId,pluginid) {
	write("onJoinRoom:" + JSON.stringify(dataJson))

	send(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1", null, null, function(dataJson) { this.onJoinRoomResult(dataJson,janusroomid,name,url,sessionId,pluginid) }, this.onError, this);
}

// ------------------------------------------
// Janus callback for Video Room Joined
// ------------------------------------------
JanusVideoRoom.prototype.onJoinRoomResult = function(dataJson,janusroomid,name,url,sessionId,pluginid) {
	write("onJoinRoomResult:" + JSON.stringify(dataJson))

	if (dataJson.plugindata.data.videoroom === "joined") {	
		// register connection
		this.connection[janusroomid + "_" + url + "_" + name] = {"sessionId":sessionId, "pluginId": pluginid };
		
		// notify new state
		this.callback(name, "joined");
		
		var peerid = Math.random().toString();
		send(this.srvurl + "/createOffer?peerid="+ peerid+"&url="+encodeURIComponent(url), null, null, function(dataJson) { this.onCreateOffer(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this); 
	} else {
		this.callback(name, "joining room failed");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Offer 
// ------------------------------------------
JanusVideoRoom.prototype.onCreateOffer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onCreateOffer:" + JSON.stringify(dataJson))
	
	this.callback(name, "publishing");
	
	var msg = { "janus": "message", "body": {"request": "publish", "video": true, "audio": false}, "jsep": dataJson, "transaction": Math.random().toString() };		
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg,  function(dataJson) { this.onPublishStream(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this);
}

// ------------------------------------------
// Janus callback for WebRTC stream is published
// ------------------------------------------
JanusVideoRoom.prototype.onPublishStream = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onPublishStream:" + JSON.stringify(dataJson))	

	send(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1", null, null, function(dataJson) { this.onPublishStreamResult(dataJson,name,peerid,sessionId,pluginid); }, this.onError, this  );
}

// ------------------------------------------
// Janus callback for WebRTC stream is published
// ------------------------------------------
JanusVideoRoom.prototype.onPublishStreamResult = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onPublishStreamResult:" + JSON.stringify(dataJson))	

	if (dataJson.jsep) {
		send(this.srvurl + "/setAnswer?peerid="+ peerid, null, dataJson.jsep, function(dataJson) { this.onSetAnswer(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this); 						
	} else {
		this.callback(name, "publishing failed (no SDP)");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Answer 
// ------------------------------------------
JanusVideoRoom.prototype.onSetAnswer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onSetAnswer:" + JSON.stringify(dataJson))	
	
	send(this.srvurl + "/getIceCandidate?peerid="+peerid, null, null, function(dataJson) { this.onReceiveCandidate(dataJson,name,sessionId,pluginid) }, this.onError, this);		
}

// ------------------------------------------
// WebRTC streamer callback for ICE candidate 
// ------------------------------------------
JanusVideoRoom.prototype.onReceiveCandidate = function(dataJson,name,sessionId,pluginid) {
	write("onReceiveCandidate answer:" + JSON.stringify(dataJson))	
	
	for (var i=0; i<dataJson.length; i++) {
		// send ICE candidate to Janus
		var msg = { "janus": "trickle", "candidate": dataJson[i], "transaction": Math.random().toString()  };
		send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg);		
	}
	
	// start long polling
	this.longpoll(null, name, sessionId);	
}

// ------------------------------------------
// Janus callback for keepAlive Session
// ------------------------------------------
JanusVideoRoom.prototype.keepAlive = function(sessionId) {
	var msg = { "janus": "keepalive", "session_id": sessionId, "transaction": Math.random().toString()  };
	send(this.janusUrl + "/" + sessionId, null, msg, function(dataJson) { write("keepAlive :" + JSON.stringify(dataJson)); }, this.onError, this);	
}

// ------------------------------------------
// Janus callback for Long Polling
// ------------------------------------------
JanusVideoRoom.prototype.longpoll = function(dataJson, name, sessionId) {
	if (dataJson) {
		write("poll evt:" + JSON.stringify(dataJson))		
	
		if (dataJson.janus == "webrtcup") {
			// notify connection
			this.callback(name, "up");
			
			// start keep alive
			var bind = this;
			setInterval( function() { bind.keepAlive(sessionId); }, 10000);	
		}
		else if (dataJson.janus == "hangup") {
			// notify connection
			this.callback(name, "down");
		}
	}
	
	send(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1", null, null, function(dataJson) { this.longpoll(dataJson, name, sessionId) }, function(dataJson) { this.longpoll(dataJson, name, sessionId) }, this);
}
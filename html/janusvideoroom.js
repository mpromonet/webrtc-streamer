function write(text) {
	console.log(text);
}

function JanusVideoRoom (janusUrl, janusRoomId, callback) {	
	this.janusUrl    = janusUrl;
	this.janusRoomId = janusRoomId;	
	this.callback    = callback;	
}
	
JanusVideoRoom.prototype.connect = function(url,name) {
	// create a session
	var createReq = {janus: "create", transaction: Math.random().toString() }
	send(this.janusUrl, null, createReq, function(dataJson) { this.onCreateSession(dataJson,url,name) }, this.onError, this);		
}

JanusVideoRoom.prototype.onCreateSession = function(dataJson,url,name) {
	var sessionId = dataJson.data.id;
	write("onCreateSession sessionId:" + sessionId);
	
	// attach to video room plugin
	var attach = { "janus": "attach", "plugin": "janus.plugin.videoroom", "transaction": Math.random().toString() };			
	send(this.janusUrl + "/" + sessionId, null, attach, function(dataJson) { this.onPluginsAttached(dataJson,url,name,sessionId) }, this.onError, this );
}
	
JanusVideoRoom.prototype.onPluginsAttached = function(dataJson,url,name,sessionId) {
	var pluginid = dataJson.data.id;
	write("onPluginsAttached pluginid:" + pluginid)	
	
	this.callback(name, "joining room");

	var join = {"janus":"message","body":{"request":"join","room":this.janusRoomId,"ptype":"publisher","display":name},"transaction":Math.random().toString()}
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, join, function(dataJson) { this.onJoinRoom(dataJson,name,url,sessionId,pluginid) }, this.onError, this );		
}

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

JanusVideoRoom.prototype.onCreateOffer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onCreateOffer:" + JSON.stringify(dataJson))
	
	this.callback(name, "publishing stream");
	
	var msg = { "janus": "message", "body": {"request": "publish", "video": true, "audio": false}, "jsep": dataJson, "transaction": Math.random().toString() };		
	send(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg,  function(dataJson) { this.onPublishStream(dataJson,name,peerid,sessionId,pluginid) }, this.onError, this);
}

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

JanusVideoRoom.prototype.onSetAnswer = function(dataJson,name,peerid,sessionId,pluginid) {
	write("onSetAnswer:" + JSON.stringify(dataJson))	
	
	send("/getIceCandidate?peerid="+peerid, null, null, function(dataJson) { this.onReceiveCandidate(dataJson,name,sessionId,pluginid) }, this.onError, this);		
}

JanusVideoRoom.prototype.onReceiveCandidate = function(dataJson,name,sessionId,pluginid) {
	write("onReceiveCandidate answer:" + JSON.stringify(dataJson))	
	
	for (var i=0; i<dataJson.length; i++) {
		var candidate = new RTCIceCandidate(dataJson[i]);

		var msg = { "janus": "trickle", "candidate": candidate, "transaction": Math.random().toString()  };
		sendSync(this.janusUrl + "/" + sessionId + "/" + pluginid, null, msg);		
	}
	
	var answer = sendSync(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1");
	write("onReceiveCandidate evt:" + JSON.stringify(answer))
	this.callback(name, "connected");

	// start long polling
	this.longpoll(null, sessionId);
	
	// start keep alive
	var bind = this;
	window.setInterval( function() { bind.keepAlive(sessionId); }, 10000);	
}

JanusVideoRoom.prototype.keepAlive = function(sessionId) {
	var msg = { "janus": "keepalive", "session_id": sessionId, "transaction": Math.random().toString()  };
	var answer = sendSync(this.janusUrl + "/" + sessionId, null, msg);
	write("keepAlive :" + JSON.stringify(answer))
}

JanusVideoRoom.prototype.longpoll = function(dataJson, sessionId) {
	if (dataJson) {
		write("poll evt:" + JSON.stringify(dataJson))		
	}
	send(this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1", null, null, function(dataJson) { this.longpoll(dataJson,sessionId) }, function(dataJson) { this.poll(dataJson,sessionId) }, this);
}

	







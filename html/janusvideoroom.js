var JanusVideoRoom = (function() {

	
/** 
 * Interface with Janus Gateway Video Room and WebRTC-streamer API
 * @constructor
 * @param {string} janusUrl - url of Janus Gateway
 * @param {string} srvurl - url of WebRTC-streamer
*/
var JanusVideoRoom = function JanusVideoRoom (janusUrl, srvurl) {	
	this.janusUrl    = janusUrl;
	this.handlers    = [];
	this.srvurl      = srvurl || location.protocol+"//"+window.location.hostname+":"+window.location.port;
	this.connection  = [];
};
	
/** 
* Ask to publish a stream from WebRTC-streamer in a Janus Video Room user
 * @param {string} janusroomid - id of the Janus Video Room to join
 * @param {string} url - WebRTC stream to publish
 * @param {string} name - name in  Janus Video Room
*/
JanusVideoRoom.prototype.join = function(janusroomid, url, name) {
	// create a session
	var createReq = {janus: "create", transaction: Math.random().toString() };

		var bind = this;
		request("POST" , this.janusUrl,
			{	
				body: JSON.stringify(createReq),
			}).done( function (response) { 
				if (response.statusCode === 200) {
					bind.onCreateSession(JSON.parse(response.body), janusroomid, url, name);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);
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
		
		var leaveReq = { "janus": "message", "body": {"request": "unpublish"}, "transaction": Math.random().toString() };
		
		var bind = this;
		request("POST" , this.janusUrl + "/" + sessionId + "/" + pluginid,
			{	
				body: JSON.stringify(leaveReq),
			}).done( function (response) { 
				if (response.statusCode === 200) {
					console.log("leave janus room answer:" + response.body);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);
	}
}

/**
* subscribeEvents
 * @param {string} fn - funtcion to call
*/
JanusVideoRoom.prototype.subscribeEvents = function(fn) {
	this.handlers.push(fn);
}

// ------------------------------------------
// callback 
// ------------------------------------------
JanusVideoRoom.prototype.callback = function(name, state) {
	this.handlers.forEach(function(item) { 
		item(name,state);
	});
}

// ------------------------------------------
// Janus callback for Session Creation
// ------------------------------------------
JanusVideoRoom.prototype.onCreateSession = function(dataJson, janusroomid, url, name) {
	var sessionId = dataJson.data.id;
	console.log("onCreateSession sessionId:" + sessionId);
	
	// attach to video room plugin
	var attachReq = { "janus": "attach", "plugin": "janus.plugin.videoroom", "transaction": Math.random().toString() };			
	
	var bind = this;
	request("POST" , this.janusUrl + "/" + sessionId,
		{	
			body: JSON.stringify(attachReq),
		}).done( function (response) { 
			if (response.statusCode === 200) {
				bind.onPluginsAttached(JSON.parse(response.body), janusroomid, url, name, sessionId);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);
}
	
// ------------------------------------------
// Janus callback for Video Room Plugins Connection
// ------------------------------------------
JanusVideoRoom.prototype.onPluginsAttached = function(dataJson, janusroomid, url, name, sessionId) {
	var pluginid = dataJson.data.id;
	console.log("onPluginsAttached pluginid:" + pluginid);
	
	this.callback(name, "joining");

	var joinReq = {"janus":"message","body":{"request":"join","room":janusroomid,"ptype":"publisher","display":name},"transaction":Math.random().toString()};
	
	var bind = this;
	request("POST" , this.janusUrl + "/" + sessionId + "/" + pluginid,
		{	
			body: JSON.stringify(joinReq),
		}).done( function (response) { 
			if (response.statusCode === 200) {
				bind.onJoinRoom(JSON.parse(response.body), janusroomid, url, name, sessionId, pluginid);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);
}

// ------------------------------------------
// Janus callback for Video Room Joined
// ------------------------------------------
JanusVideoRoom.prototype.onJoinRoom = function(dataJson,janusroomid,url,name,sessionId,pluginid) {
	console.log("onJoinRoom:" + JSON.stringify(dataJson));

	var bind = this;
	request("GET" , this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1")
		.done( function (response) { 
			if (response.statusCode === 200) {
				bind.onJoinRoomResult(JSON.parse(response.body), janusroomid, url, name, sessionId, pluginid);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);
}

// ------------------------------------------
// Janus callback for Video Room Joined
// ------------------------------------------
JanusVideoRoom.prototype.onJoinRoomResult = function(dataJson,janusroomid,url,name,sessionId,pluginid) {
	console.log("onJoinRoomResult:" + JSON.stringify(dataJson));

	if (dataJson.plugindata.data.videoroom === "joined") {	
		// register connection
		this.connection[janusroomid + "_" + url + "_" + name] = {"sessionId":sessionId, "pluginId": pluginid };
		
		// notify new state
		this.callback(name, "joined");
		
		var peerid = Math.random().toString();
		
		var bind = this;
		request("GET" , this.srvurl + "/api/createOffer?peerid="+ peerid+"&url="+encodeURIComponent(url))
			.done( function (response) { 
				if (response.statusCode === 200) {
					bind.onCreateOffer(JSON.parse(response.body), name, sessionId, pluginid, peerid);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);		
	} else {
		this.callback(name, "joining room failed");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Offer 
// ------------------------------------------
JanusVideoRoom.prototype.onCreateOffer = function(dataJson,name,sessionId,pluginid,peerid) {
	console.log("onCreateOffer:" + JSON.stringify(dataJson));
	
	this.callback(name, "publishing");
	
	var publishReq = { "janus": "message", "body": {"request": "publish", "video": true, "audio": true, "data": true}, "jsep": dataJson, "transaction": Math.random().toString() };		
	var bind = this;
	request("POST" , this.janusUrl + "/" + sessionId + "/" + pluginid,
		{	
			body: JSON.stringify(publishReq),
		}).done( function (response) { 
			if (response.statusCode === 200) {
				bind.onPublishStream(JSON.parse(response.body), name, sessionId, pluginid, peerid);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);		
}

// ------------------------------------------
// Janus callback for WebRTC stream is published
// ------------------------------------------
JanusVideoRoom.prototype.onPublishStream = function(dataJson,name,sessionId,pluginid,peerid) {
	console.log("onPublishStream:" + JSON.stringify(dataJson));

	var bind = this;
	request("GET" , this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1")
		.done( function (response) { 
			if (response.statusCode === 200) {
				bind.onPublishStreamResult(JSON.parse(response.body), name, sessionId, pluginid, peerid);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);		
}

// ------------------------------------------
// Janus callback for WebRTC stream is published
// ------------------------------------------
JanusVideoRoom.prototype.onPublishStreamResult = function(dataJson,name,sessionId,pluginid,peerid) {
	console.log("onPublishStreamResult:" + JSON.stringify(dataJson));

	if (dataJson.jsep) {
		var bind = this;
		request("POST" , this.srvurl + "/api/setAnswer?peerid="+ peerid,
			{	
				body: JSON.stringify(dataJson.jsep),
			}).done( function (response) { 
				if (response.statusCode === 200) {
					bind.onSetAnswer(JSON.parse(response.body), name, sessionId, pluginid, peerid);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);		
	} else {
		this.callback(name, "publishing failed (no SDP)");
	}
}

// ------------------------------------------
// WebRTC streamer callback for Answer 
// ------------------------------------------
JanusVideoRoom.prototype.onSetAnswer = function(dataJson,name,sessionId,pluginid,peerid) {
	console.log("onSetAnswer:" + JSON.stringify(dataJson));
	
	var bind = this;
	request("GET" , this.srvurl + "/api/getIceCandidate?peerid="+peerid)
		.done( function (response) { 
			if (response.statusCode === 200) {
				bind.onReceiveCandidate(JSON.parse(response.body), name, sessionId, pluginid);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);		
}

// ------------------------------------------
// WebRTC streamer callback for ICE candidate 
// ------------------------------------------
JanusVideoRoom.prototype.onReceiveCandidate = function(dataJson,name,sessionId,pluginid) {
	console.log("onReceiveCandidate answer:" + JSON.stringify(dataJson));
	
	for (var i=0; i<dataJson.length; i++) {
		// send ICE candidate to Janus
		var candidateReq = { "janus": "trickle", "candidate": dataJson[i], "transaction": Math.random().toString()  };
		
		var bind = this;
		request("POST" , this.janusUrl + "/" + sessionId + "/" + pluginid,
			{	
				body: JSON.stringify(candidateReq),
			}).done( function (response) { 
				if (response.statusCode === 200) {
					console.log("onReceiveCandidate janus answer:" + response.body);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);		
	}
	
	// start long polling
	this.longpoll(null, name, sessionId);	
}

// ------------------------------------------
// Janus callback for keepAlive Session
// ------------------------------------------
JanusVideoRoom.prototype.keepAlive = function(sessionId) {
	var keepAliveReq = { "janus": "keepalive", "session_id": sessionId, "transaction": Math.random().toString()  };
	
	var bind = this;
	request("POST" , this.janusUrl + "/" + sessionId,
		{	
			body: JSON.stringify(keepAliveReq),
		}).done( function (response) { 
			if (response.statusCode === 200) {
				console.log("keepAlive:" + response.body);
			}
			else {
				bind.onError(response.statusCode);
			}
		}
	);		
}

// ------------------------------------------
// Janus callback for Long Polling
// ------------------------------------------
JanusVideoRoom.prototype.longpoll = function(dataJson, name, sessionId) {
	if (dataJson) {
		console.log("poll evt:" + JSON.stringify(dataJson));
	
		if (dataJson.janus === "webrtcup") {
			// notify connection
			this.callback(name, "up");
			
			// start keep alive
			var bind = this;
			setInterval( function() { bind.keepAlive(sessionId); }, 10000);	
		}
		else if (dataJson.janus === "hangup") {
			// notify connection
			this.callback(name, "down");
		}
	}
	
	var bind = this;
	request("GET" , this.janusUrl + "/" + sessionId + "?rid=" + new Date().getTime() + "&maxev=1")
		.done( function (response) { 
			bind.longpoll( JSON.parse(response.body), name, sessionId);
		}
	);		
}

// ------------------------------------------
// Janus callback for Error
// ------------------------------------------
JanusVideoRoom.prototype.onError = function(status) {
	console.log("onError:" + status);
}


return JanusVideoRoom;
})();

module.exports = JanusVideoRoom;

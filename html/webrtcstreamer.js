
RTCPeerConnection = window.RTCPeerConnection || window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
RTCSessionDescription = window.RTCSessionDescription || window.mozRTCSessionDescription || window.webkitRTCSessionDescription;
RTCIceCandidate = window.RTCIceCandidate || window.mozRTCIceCandidate || window.webkitRTCIceCandidate;
URL = window.URL || window.webkitURL;

/** 
 * Interface with WebRTC-streamer API
 * @constructor
 * @param {string} videoElement - id of the video element tag
 * @param {string} srvurl -  url of webrtc-streamer (default is current location)
*/
function WebRtcStreamer (videoElement, srvurl, request) {
	this.videoElement     = videoElement;	
	this.srvurl           = srvurl || location.protocol+"//"+window.location.hostname+":"+window.location.port;
	this.pc               = null;    

	this.pcOptions        = { "optional": [{"DtlsSrtpKeyAgreement": true} ] };

	this.mediaConstraints = {};
	if (navigator.userAgent.indexOf("Firefox") > 0) {
		this.mediaConstraints = {"offerToReceiveVideo": true, "offerToReceiveAudio": true  };
	}
	else {
		this.mediaConstraints = {"mandatory": {"OfferToReceiveVideo": true, "OfferToReceiveAudio": true }};
	}

	this.iceServers = null;
	this.request  = request;
	this.earlyCandidates = [];
}

/** 
 * Connect a WebRTC Stream to videoElement 
 * @param {string} videourl - id of WebRTC video stream
 * @param {string} audiourl - id of WebRTC audio stream
 * @param {string} options -  options of WebRTC call
 * @param {string} stream  -  local stream to send
*/
WebRtcStreamer.prototype.connect = function(videourl, audiourl, options, localstream) {
	this.disconnect();
	
	// getIceServers is not already received
	if (!this.iceServers) {
		console.log("Get IceServers");
		sendRequest(this.request, this.srvurl + "/getIceServers", null, null, function(iceServers) { this.onReceiveGetIceServers(iceServers, videourl, audiourl, options, localstream); } , null, this);
	} else {
		this.onGetIceServers(this.iceServers, videourl, audiourl, options, localstream);
	}
}

/** 
 * Disconnect a WebRTC Stream and clear videoElement source
*/
WebRtcStreamer.prototype.disconnect = function() {		
	var videoElement = document.getElementById(this.videoElement);
	if (videoElement) {
		videoElement.src = "";
	}
	if (this.pc) {
		sendRequest(this.request, this.srvurl + "/hangup?peerid="+this.pc.peerid);
		try {
			this.pc.close();
		}
		catch (e) {
			console.log ("Failure close peer connection:" + e);
		}
		this.pc = null;
	}
}    

/*
* GetIceServers callback
*/
WebRtcStreamer.prototype.onReceiveGetIceServers = function(iceServers, videourl, audiourl, options, stream) {
	this.iceServers       = iceServers;
	this.pcConfig         = iceServers || {"iceServers": [] };
	try {            
		this.pc = this.createPeerConnection();

		var peerid = Math.random();			
		this.pc.peerid = peerid;
		
		var streamer = this;
		var callurl = this.srvurl + "/call?peerid="+ peerid+"&url="+encodeURIComponent(videourl);
		if (audiourl) {
			callurl += "&audiourl="+encodeURIComponent(audiourl);
		}
		if (options) {
			callurl += "&options="+encodeURIComponent(options);
		}
		
		if (stream) {
			this.pc.addStream(stream);
		}

                // clear early candidates
		this.earlyCandidates.length = 0;
		
		// create Offer
		this.pc.createOffer(function(sessionDescription) {
			console.log("Create offer:" + JSON.stringify(sessionDescription));
			
			streamer.pc.setLocalDescription(sessionDescription
				, function() { sendRequest(streamer.request, callurl, null, sessionDescription, streamer.onReceiveCall, null, streamer); }
				, function() {} );
			
		}, function(error) { 
			alert("Create offer error:" + JSON.stringify(error));
		}, this.mediaConstraints); 															

	} catch (e) {
		this.disconnect();
		alert("connect error: " + e);
	}	    
}

/*
* create RTCPeerConnection 
*/
WebRtcStreamer.prototype.createPeerConnection = function() {
	console.log("createPeerConnection  config: " + JSON.stringify(this.pcConfig) + " option:"+  JSON.stringify(this.pcOptions));
	var pc = new RTCPeerConnection(this.pcConfig, this.pcOptions);
	var streamer = this;
	pc.onicecandidate = function(evt) { streamer.onIceCandidate.call(streamer, evt); };
	if (typeof pc.ontrack != "undefined") {
		pc.ontrack        = function(evt) { streamer.onTrack.call(streamer,evt); };
	} 
	else {
		pc.onaddstream    = function(evt) { streamer.onTrack.call(streamer,evt); };
	}
	pc.oniceconnectionstatechange = function(evt) {  
		console.log("oniceconnectionstatechange  state: " + pc.iceConnectionState);
		var videoElement = document.getElementById(streamer.videoElement);
		if (videoElement) {
			if (pc.iceConnectionState === "connected") {
				videoElement.style.opacity = "1.0";
			}			
			else if (pc.iceConnectionState === "disconnected") {
				videoElement.style.opacity = "0.25";
			}			
			else if ( (pc.iceConnectionState === "failed") || (pc.iceConnectionState === "closed") )  {
				videoElement.style.opacity = "0.5";
			}			
		}
	}
	pc.ondatachannel = function(evt) {  
		console.log("remote datachannel created:"+JSON.stringify(evt));
		
		evt.channel.onopen = function () {
			console.log("remote datachannel open");
			this.send("remote channel openned");
		}
		evt.channel.onmessage = function (event) {
			console.log("remote datachannel recv:"+JSON.stringify(event.data));
		}
	}

	var dataChannel = pc.createDataChannel("ClientDataChannel");
	dataChannel.onopen = function() {
		console.log("local datachannel open");
		this.send("local channel openned");
	}
	dataChannel.onmessage = function(evt) {
		console.log("local datachannel recv:"+JSON.stringify(evt.data));
	}
	
	console.log("Created RTCPeerConnnection with config: " + JSON.stringify(this.pcConfig) + "option:"+  JSON.stringify(this.pcOptions) );
	return pc;
}


/*
* RTCPeerConnection IceCandidate callback
*/
WebRtcStreamer.prototype.onIceCandidate = function (event) {
	if (event.candidate) {
                if (this.pc.currentRemoteDescription)  {
			sendRequest(this.request, this.srvurl + "/addIceCandidate?peerid="+this.pc.peerid, null, event.candidate);
		} else {
			this.earlyCandidates.push(event.candidate);
		}
	} 
	else {
		console.log("End of candidates.");
	}
}

/*
* RTCPeerConnection AddTrack callback
*/
WebRtcStreamer.prototype.onTrack = function(event) {
	console.log("Remote track added:" +  JSON.stringify(event));
        var stream;
	if (event.streams) {
		stream = event.streams[0];
	} 
	else {
		stream = event.stream;
	}
	var videoElement = document.getElementById(this.videoElement);
	videoElement.src = URL.createObjectURL(stream);
	videoElement.play();
}
		
/*
* AJAX /call callback
*/
WebRtcStreamer.prototype.onReceiveCall = function(dataJson) {
	var streamer = this;
	console.log("offer: " + JSON.stringify(dataJson));
	this.pc.setRemoteDescription(new RTCSessionDescription(dataJson)
		, function()      { console.log ("setRemoteDescription ok") 
                        while (streamer.earlyCandidates.length) {
			        var candidate = streamer.earlyCandidates.shift();
				sendRequest(streamer.request, streamer.srvurl + "/addIceCandidate?peerid="+streamer.pc.peerid, null, candidate);
			}
			sendRequest(streamer.request, streamer.srvurl + "/getIceCandidate?peerid="+streamer.pc.peerid, null, null, streamer.onReceiveCandidate, null, streamer);
		}
		, function(error) { console.log ("setRemoteDescription error:" + JSON.stringify(error)); });
}	

/*
* AJAX /getIceCandidate callback
*/
WebRtcStreamer.prototype.onReceiveCandidate = function(dataJson) {
	console.log("candidate: " + JSON.stringify(dataJson));
	if (dataJson) {
		for (var i=0; i<dataJson.length; i++) {
			var candidate = new RTCIceCandidate(dataJson[i]);
			
			console.log("Adding ICE candidate :" + JSON.stringify(candidate) );
			this.pc.addIceCandidate(candidate
				, function()      { console.log ("addIceCandidate OK"); }
				, function(error) { console.log ("addIceCandidate error:" + JSON.stringify(error)); } );
		}
	}
}

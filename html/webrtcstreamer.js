
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
function WebRtcStreamer (videoElement, srvurl) {
	this.videoElement     = videoElement;	
	this.srvurl           = srvurl || location.protocol+"//"+window.location.hostname+":"+window.location.port;
	this.pc               = null;    

	this.pcOptions        = { 'optional': [{'DtlsSrtpKeyAgreement': true} ] };

	this.mediaConstraints = {}
	if (navigator.userAgent.indexOf("Firefox") > 0) {
		this.mediaConstraints = {'offerToReceiveVideo': true, 'offerToReceiveAudio': true  };
	}
	else {
		this.mediaConstraints = {'mandatory': {'OfferToReceiveVideo': true, 'OfferToReceiveAudio': true }}
	}

	// getIceServers
	var iceServers = sendSync('/getIceServers');
	this.pcConfig         = iceServers || {'iceServers': [] };
}
 	
/** 
 * Connect a WebRTC Stream to videoElement 
 * @param {string} videourl - id of WebRTC video stream
 * @param {string} audiourl - id of WebRTC audio stream
 * @param {string} options -  options of WebRTC call
*/
WebRtcStreamer.prototype.connect = function(videourl, audiourl, options) {
	this.disconnect();
	
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
		
		// create Offer
		this.pc.createOffer(function(sessionDescription) {
			trace("Create offer:" + JSON.stringify(sessionDescription));
			
			streamer.pc.setLocalDescription(sessionDescription
				, function() { send(callurl, null, sessionDescription, streamer.onReceiveCall, null, streamer); }
				, function() {} );
			
		}, function(error) { 
			alert("Create offer error:" + JSON.stringify(error));
		}, this.mediaConstraints); 															

	} catch (e) {
		this.disconnect();
		alert("connect error: " + e);
	}	    
}

/** 
 * Disconnect a WebRTC Stream and clear videoElement source
*/
WebRtcStreamer.prototype.disconnect = function() {		
	var videoElement = document.getElementById(this.videoElement);
	if (videoElement) {
		videoElement.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=';
	}
	if (this.pc) {
		send(this.srvurl + "/hangup?peerid="+this.pc.peerid);
		try {
			this.pc.close();
		}
		catch (e) {
			trace ("Failure close peer connection:" + e);
		}
		this.pc = null;
	}
}    

/*
* create RTCPeerConnection 
*/
WebRtcStreamer.prototype.createPeerConnection = function() {
	trace("createPeerConnection  config: " + JSON.stringify(this.pcConfig) + " option:"+  JSON.stringify(this.pcOptions));
	var pc = new RTCPeerConnection(this.pcConfig, this.pcOptions);
	var streamer = this;
	pc.onicecandidate = function(evt) { streamer.onIceCandidate.call(streamer, evt) };
	if (typeof pc.ontrack != "undefined") {
		pc.ontrack        = function(evt) { streamer.onTrack.call(streamer,evt) };
	} 
	else {
		pc.onaddstream    = function(evt) { streamer.onTrack.call(streamer,evt) };
	}
	pc.oniceconnectionstatechange = function(evt) {  
		trace("oniceconnectionstatechange  state: " + pc.iceConnectionState);
		var videoElement = document.getElementById(streamer.videoElement);
		if (videoElement) {
			if (pc.iceConnectionState == "connected") {
				videoElement.style.opacity = "1.0";
			}			
			else if (pc.iceConnectionState == "disconnected") {
				videoElement.style.opacity = "0.25";
			}			
			else if ( (pc.iceConnectionState == "failed") || (pc.iceConnectionState == "closed") )  {
				videoElement.style.opacity = "0.5";
			}			
		}
	}
	
	trace("Created RTCPeerConnnection with config: " + JSON.stringify(this.pcConfig) + "option:"+  JSON.stringify(this.pcOptions) );
	return pc;
}

/*
* RTCPeerConnection IceCandidate callback
*/
WebRtcStreamer.prototype.onIceCandidate = function (event) {
	if (event.candidate) {
		send(this.srvurl + "/addIceCandidate?peerid="+this.pc.peerid, null, event.candidate);
	} 
	else {
		trace("End of candidates.");
		send  (this.srvurl + "/getIceCandidate?peerid="+this.pc.peerid, null, null, this.onReceiveCandidate, null, this);
	}
}

/*
* RTCPeerConnection AddTrack callback
*/
WebRtcStreamer.prototype.onTrack = function(event) {
	trace("Remote track added:" +  JSON.stringify(event));
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
	trace("offer: " + JSON.stringify(dataJson));
	var peerid = this.pc.peerid;
	this.pc.setRemoteDescription(new RTCSessionDescription(dataJson)
		, function()      { trace ("setRemoteDescription ok") }
		, function(error) { trace ("setRemoteDescription error:" + JSON.stringify(error)); });
}	

/*
* AJAX /getIceCandidate callback
*/
WebRtcStreamer.prototype.onReceiveCandidate = function(dataJson) {
	trace("candidate: " + JSON.stringify(dataJson));
	if (dataJson) {
		for (var i=0; i<dataJson.length; i++) {
			var candidate = new RTCIceCandidate(dataJson[i]);
			
			trace("Adding ICE candidate :" + JSON.stringify(candidate) );
			this.pc.addIceCandidate(candidate
				, function()      { trace ("addIceCandidate OK"); }
				, function(error) { trace ("addIceCandidate error:" + JSON.stringify(error)); } );
		}
	}
}

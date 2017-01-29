
RTCPeerConnection = window.RTCPeerConnection || window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
RTCSessionDescription = window.RTCSessionDescription || window.mozRTCSessionDescription || window.RTCSessionDescription;
RTCIceCandidate = window.RTCIceCandidate || window.mozRTCIceCandidate || window.RTCIceCandidate;
URL = window.webkitURL || window.URL;


// ------------------------------------------
// Constructor
// ------------------------------------------
function WebRtcStreamer (videoElement, srvurl) {
	this.videoElement     = videoElement;	
	this.srvurl           = srvurl || location.protocol+"//"+window.location.hostname+":"+window.location.port;
	this.pc               = null;    
	this.pcConfig         = {'iceServers': [] };
	this.pcOptions        = { 'optional': [{'DtlsSrtpKeyAgreement': true} ] };
	this.mediaConstraints = {}
	if (navigator.userAgent.indexOf("Firefox") > 0) {
		this.mediaConstraints = {'offerToReceiveVideo': true };
	}
	else {
		this.mediaConstraints = {'mandatory': {'OfferToReceiveVideo': true }}
	}
}
 
// ------------------------------------------
// create RTCPeerConnection 
// ------------------------------------------
WebRtcStreamer.prototype.createPeerConnection = function() {
	var pc = null;
	try {
		trace("createPeerConnection  config: " + JSON.stringify(this.pcConfig) + " option:"+  JSON.stringify(this.pcOptions));
		pc = new RTCPeerConnection(this.pcConfig, this.pcOptions);
		var streamer = this;
		pc.onicecandidate = function(evt) { streamer.onIceCandidate.call(streamer, evt) };
		if (typeof pc.ontrack != "undefined") {
			pc.ontrack        = function(evt) { streamer.onTrack.call(streamer,evt) };
		} 
		else {
			pc.onaddstream    = function(evt) { streamer.onTrack.call(streamer,evt) };
		}
		trace("Created RTCPeerConnnection with config: " + JSON.stringify(this.pcConfig) + "option:"+  JSON.stringify(this.pcOptions) );
	} 
	catch (e) {
		trace("Failed to create PeerConnection exception: " + e.message);
	}
	return pc;
}
		
// ------------------------------------------
// RTCPeerConnection IceCandidate callback
// ------------------------------------------
WebRtcStreamer.prototype.onIceCandidate = function (event) {
	if (event.candidate) {
		send(this.srvurl + "/addIceCandidate?peerid="+this.pc.peerid,null,JSON.stringify(event.candidate));
	} 
	else {
		trace("End of candidates.");
	}
}

// ------------------------------------------
// RTCPeerConnection AddTrack callback
// ------------------------------------------
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
		
// ------------------------------------------
// AJAX /call callback
// ------------------------------------------		
WebRtcStreamer.prototype.onReceiveCall = function(request) {
	var streamer = this;
	trace("offer: " + request.responseText);
	var dataJson = JSON.parse(request.responseText);
	var peerid = this.pc.peerid;
	this.pc.setRemoteDescription(new RTCSessionDescription(dataJson)
		, function()      { send  (streamer.srvurl + "/getIceCandidate?peerid="+peerid, null, null, streamer.onReceiveCandidate, null, streamer); }
		, function(error) { trace ("setRemoteDescription error:" + JSON.stringify(error)); });
}	

// ------------------------------------------
// AJAX /getIceCandidate callback
// ------------------------------------------		
WebRtcStreamer.prototype.onReceiveCandidate = function(request) {
	trace("candidate: " + request.responseText);
	var dataJson = JSON.parse(request.responseText);
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

// ------------------------------------------
// Connect to WebRtc Stream
// ------------------------------------------	
WebRtcStreamer.prototype.connect = function(url) {
	this.disconnect();
	
	try {            
		this.pc = this.createPeerConnection();
		var peerid = Math.random()*65535;			
		this.pc.peerid = peerid;
		
		var streamer = this;
		// create Offer
		this.pc.createOffer(function(sessionDescription) {
			trace("Create offer:" + JSON.stringify(sessionDescription));
			
			// Create call body adding url to offer
			var callJson = sessionDescription.toJSON();
			callJson.url = url;
			
			streamer.pc.setLocalDescription(sessionDescription
				, function() { send(streamer.srvurl + "/call?peerid="+ peerid, null, JSON.stringify(callJson), streamer.onReceiveCall, null, streamer); }
				, function() {} );
			
		}, function(error) { 
			alert("Create offer error:" + JSON.stringify(error));
		}, this.mediaConstraints); 															

	} catch (e) {
		this.disconnect();
		alert("connect error: " + e);
	}	    
}

// ------------------------------------------
// Disconnect from a WebRtc Stream
// ------------------------------------------	
WebRtcStreamer.prototype.disconnect = function() {		
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
	var videoElement = document.getElementById(this.videoElement);
	videoElement.src = 'data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';
}    

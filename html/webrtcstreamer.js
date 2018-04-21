
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

	this.pcOptions        = { "optional": [{"DtlsSrtpKeyAgreement": true} ] };

	this.mediaConstraints = { offerToReceiveAudio: true, offerToReceiveVideo: true };

	this.iceServers = null;
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
		
		var bind = this;
		request("GET" , this.srvurl + "/api/getIceServers")
			.done( function (response) { 
				if (response.statusCode === 200) {
					bind.onReceiveGetIceServers.call(bind,JSON.parse(response.body), videourl, audiourl, options, localstream);
				}
				else {
					bind.onError(response.statusCode);
				}
			}
		);		
	} else {
		this.onReceiveGetIceServers(this.iceServers, videourl, audiourl, options, localstream);
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
		request("GET" , this.srvurl + "/api/hangup?peerid="+this.pc.peerid);
		
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
		
		var callurl = this.srvurl + "/api/call?peerid="+ peerid+"&url="+encodeURIComponent(videourl);
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
		var bind = this;
		this.pc.createOffer(this.mediaConstraints).then(function(sessionDescription) {
			console.log("Create offer:" + JSON.stringify(sessionDescription));
			
			bind.pc.setLocalDescription(sessionDescription
				, function() {
					request("POST" , callurl, { body: JSON.stringify(sessionDescription) })
						.done( function (response) { 
							if (response.statusCode === 200) {
								bind.onReceiveCall.call(bind,JSON.parse(response.body));
							}
							else {
								bind.onError(response.statusCode);
							}
						}
					);					
				}
				, function() {} );
			
		}, function(error) { 
			alert("Create offer error:" + JSON.stringify(error));
		});

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
	pc.onaddstream    = function(evt) { streamer.onAddStream.call(streamer,evt); };
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

	try {
		var dataChannel = pc.createDataChannel("ClientDataChannel");
		dataChannel.onopen = function() {
			console.log("local datachannel open");
			this.send("local channel openned");
		}
		dataChannel.onmessage = function(evt) {
			console.log("local datachannel recv:"+JSON.stringify(evt.data));
		}
	} catch (e) {
		console.log("Cannor create datachannel error: " + e);
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
			var bind = this;
			request("POST" , this.srvurl + "/api/addIceCandidate?peerid="+this.pc.peerid, { body: JSON.stringify(event.candidate) })
				.done( function (response) { 
					if (response.statusCode === 200) {
						console.log("addIceCandidate ok:" + response.body);
					}
					else {
						bind.onError(response.statusCode);
					}
				}
			);					
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
WebRtcStreamer.prototype.onAddStream = function(event) {
	console.log("Remote track added:" +  JSON.stringify(event));
	
	var videoElement = document.getElementById(this.videoElement);
	videoElement.srcObject = event.stream;
	videoElement.setAttribute("playsinline", true);
	videoElement.play();
}
		
/*
* AJAX /call callback
*/
WebRtcStreamer.prototype.onReceiveCall = function(dataJson) {
	var bind = this;
	console.log("offer: " + JSON.stringify(dataJson));
	this.pc.setRemoteDescription(new RTCSessionDescription(dataJson)
		, function()      { 
                        console.log ("setRemoteDescription ok");
                        while (bind.earlyCandidates.length) {
				var candidate = bind.earlyCandidates.shift();
				
				request("POST" , bind.srvurl + "/api/addIceCandidate?peerid=" + bind.pc.peerid, { body: JSON.stringify(candidate) })
					.done( function (response) { 
						if (response.statusCode === 200) {
							console.log("addIceCandidate ok:" + response.body);
						}
						else {
							bind.onError(response.statusCode);
						}
					}
				);
			}
			
			request("GET" , bind.srvurl + "/api/getIceCandidate?peerid=" + bind.pc.peerid)
				.done( function (response) { 
					if (response.statusCode === 200) {
						bind.onReceiveCandidate.call(bind,JSON.parse(response.body));
					}
					else {
						bind.onError(response.statusCode);
					}
				}
			);
				
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


/*
* AJAX callback for Error
*/
WebRtcStreamer.prototype.onError = function(status) {
	console.log("onError:" + status);
}
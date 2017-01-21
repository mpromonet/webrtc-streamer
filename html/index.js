	var pc;    
	var pcConfig = {'iceServers': [] };
	var pcOptions = { 'optional': [{'DtlsSrtpKeyAgreement': true} ] };
	var mediaConstraints = {}
	if (navigator.userAgent.indexOf("Firefox") > 0)
	{
		mediaConstraints = {'offerToReceiveVideo': true };
	}
	else
	{
		mediaConstraints = {'mandatory': {'OfferToReceiveVideo': true }}
	}
	
	RTCPeerConnection = window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
	RTCSessionDescription = window.mozRTCSessionDescription || window.RTCSessionDescription;
	RTCIceCandidate = window.mozRTCIceCandidate || window.RTCIceCandidate;
	URL = window.webkitURL || window.URL;
	
	function trace(txt) {
		console.log(txt);
	}

	// ------------------------------------------
	// AJAX helper
	// ------------------------------------------	
	function send(method,peerid,data,onSuccess,onFailure) {
		trace("HTTP call "+ method);
		try {
			var r = new XMLHttpRequest();
			r.open("POST",method, true);
			r.setRequestHeader("Content-Type", "text/plain");
			r.setRequestHeader("peerid", peerid);
			r.onreadystatechange = function() {
				if (this.readyState == 4) {
					if ( (this.status == 200) && onSuccess ) {
						onSuccess(this);
					}
					else if (onFailure) {
						onFailure(this);
					}
				}			
			}
			r.send(data);
			r = null;
		} catch (e) {
			trace("send to peer:" + peerid + " error: " + e.description);
		}
	}

	// ------------------------------------------
	// RTCPeerConnection and its callbacks
	// ------------------------------------------
	function createPeerConnection() {
		var pc = null;
		try {
			trace("createPeerConnection  config: " + JSON.stringify(pcConfig) + " option:"+  JSON.stringify(pcOptions));
			pc = new RTCPeerConnection(pcConfig, pcOptions);
			pc.onicecandidate = onIceCandidate;
			pc.ontrack = onTrack;
			pc.onaddstream = onAddStream;
			trace("Created RTCPeerConnnection with config: " + JSON.stringify(pcConfig) + "option:"+  JSON.stringify(pcOptions) );
		} 
		catch (e) {
			trace("Failed to create PeerConnection with " + connectionId + ", exception: " + e.message);
		}
		return pc;
	}
		
	function onIceCandidate(event) {
		if (event.candidate) {
			send("/addIceCandidate",pc.peerid,JSON.stringify(event.candidate));
		} else {
			trace("End of candidates.");
		}
	}

	function onAddStream(event) {
		trace("Remote stream added: " + JSON.stringify(event));
		var remoteVideoElement = document.getElementById('remote-video');
                remoteVideoElement.src = URL.createObjectURL(event.stream);
                remoteVideoElement.play();
	}

	function onTrack(event) {
		trace("Remote stream added:" +  JSON.stringify(event));
		var remoteVideoElement = document.getElementById('remote-video');
		remoteVideoElement.src = URL.createObjectURL(event.streams[0]);
		remoteVideoElement.play();
	}
		
	// ------------------------------------------
	// AJAX callbacks
	// ------------------------------------------		
	function onReceiveCall(request) {
		trace("peerid: " + request.getResponseHeader("peerid") + " offer: " + request.responseText);
		pc.peerid = request.getResponseHeader("peerid");
		var dataJson = JSON.parse(request.responseText);
		pc.setRemoteDescription(new RTCSessionDescription(dataJson)
			, function()      { send("/getIceCandidate", pc.peerid, null, onReceiveCandidate); }
			, function(error) { trace ("setRemoteDescription error:" + JSON.stringify(error)); });
	}	

	function onReceiveCandidate(request) {
		trace("candidate: " + request.responseText);
		var dataJson = JSON.parse(request.responseText);
		if (dataJson)
		{
			for (var i=0; i<dataJson.length; i++)
			{
				var candidate = new RTCIceCandidate(dataJson[i]);
				
				trace("Adding ICE candidate :" + JSON.stringify(candidate) );
				pc.addIceCandidate(candidate
					, function()      { trace ("addIceCandidate OK"); }
					, function(error) { trace ("addIceCandidate error:" + JSON.stringify(error)); } );
			}
		}
	}

	// ------------------------------------------
	// Connect button callback
	// ------------------------------------------	
	function connect() {
		var url = document.getElementById("url").value;
			
		try {            
			pc = createPeerConnection();
			
			// create Offer
			pc.createOffer(function(sessionDescription) {
				trace("Create offer:" + JSON.stringify(sessionDescription));
				
				// Create call body adding url to offer
				var callJson = sessionDescription.toJSON();
				callJson.url = url;
				
				pc.setLocalDescription(sessionDescription
					, function() { send("/call", null, JSON.stringify(callJson), onReceiveCall); }
					, function() {} );
				
			}, function(error) { 
				alert("Create offer error:" + JSON.stringify(error));
			}, mediaConstraints); 															

		} catch (e) {
			disconnect();
			alert("connect error: " + e);
		}	    
	}
	
	// ------------------------------------------
	// Disconnect button callback
	// ------------------------------------------	
	function disconnect() {		
		if (pc) {
			send("/hangup", pc.peerid);
			try {
				pc.close();
			}
			catch (e) {
				trace ("Failure close peer connection:" + e);
			}
			pc = null;
			var remoteVideoElement = document.getElementById('remote-video');
			remoteVideoElement.src = 'data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';
		}
	}    

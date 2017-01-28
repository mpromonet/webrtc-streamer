// ------------------------------------------
// log wrapper
// ------------------------------------------	
function trace(txt) {
	console.log(txt);
}

// ------------------------------------------
// Simple AJAX helper
// ------------------------------------------	
function send(method,headers,data,onSuccess,onFailure,scope) {
	
	trace("HTTP call "+ method);
	try {
		var r = new XMLHttpRequest();
		r.open("POST",method, true);
		r.setRequestHeader("Content-Type", "text/plain");
		if (headers) {
			for (key in headers) {
				r.setRequestHeader(key, headers[key]);
			}
		}
		r.onreadystatechange = function() {
			if (this.readyState == 4) {
				if ( (this.status == 200) && onSuccess ) {
					onSuccess.call(scope,this);
				}
				else if (onFailure) {
					onFailure.call(scope,this);
				}
			}			
		}
		r.send(data);
		r = null;
	} catch (e) {
		trace("send to peer:" + peerid + " error: " + e.description);
	}
}


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
	var request = new XMLHttpRequest();
	var verb = 'GET';
	if (data) {
		verb = 'POST';
	}	
	var async = (typeof onSuccess === 'function') || (typeof onFailure === 'function');
	request.open(verb, method, async);
	if (headers) {
		for (key in headers) {
			request.setRequestHeader(key, headers[key]);
		}
	}
	if (data) {
		request.setRequestHeader("Content-Type", "text/plain");
		data = JSON.stringify(data);
	}
	if (async) {
		request.onreadystatechange = function() {
			if (this.readyState === 4) {
				if ( (this.status === 200) && onSuccess ) {
					onSuccess.call(scope,JSON.parse(this.responseText));
				}
				else if (onFailure) {
					onFailure.call(scope,this.status);
				}
			}			
		}
	}
	request.send(data);
	return request;
}

function sendSync(method,headers,data) {
	var answer = null;
	var request = send(method, headers, data);
	if (request.status === 200) {
		answer = JSON.parse(request.responseText);
	}
	return answer;
}
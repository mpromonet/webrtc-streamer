function sendRequest(request,method,headers,data,onSuccess,onFailure,scope) {

	console.log("HTTP call "+ method);
	var verb = "GET";
	if (data) {
		verb = "POST";
		data = JSON.stringify(data);
	}
	request(verb , method,
		{	
			body: data,
			headers: headers
		}).done( function (response) { 
			if (response.statusCode === 200) {
				if (onSuccess) {
					onSuccess.call(scope,JSON.parse(response.body));
				}
			}
			else if (onFailure) {
				onFailure.call(scope,response.statusCode);
			}
		}
	);
}
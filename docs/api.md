# api

The WebRTC signaling is implemented through HTTP requests:

 - /api/call   : send offer and get answer
 - /api/hangup : close a call

 - /api/addIceCandidate : add a candidate
 - /api/getIceCandidate : get the list of candidates

The list of HTTP API is available using /api/help.

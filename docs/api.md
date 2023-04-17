# api

The WebRTC signaling is implemented through HTTP requests:

 - `/api/call?peerid=<peerid>`   : send offer (through body) and get answer (JSON)
 - `/api/hangup?peerid=<peerid>` : close a call

 - `/api/createOffer` :
 - `/api/setAnswer` : 

 - `/api/getIceServers` : get object of ICE servers
 - `/api/getStreamList` : gets currently active streams
 - `/api/getVideoDeviceList` : gets list of streamable video devices
 - `/api/getAudioDeviceList` : get list of streamable audio devices
 - `/api/getPeerConnectionList` : get list of active WebRTC peer connections
- `/api/getMediaList` : get list of combined audio and video strings in objects

 - `/api/addIceCandidate?peerid=<peerId>` : add a candidate
 - `/api/getIceCandidate?peerid=<peerId>` : get the list of candidates
 
 - `/api/version` : get current webrtc-streamer version
 - `/api/log` : 

The list of HTTP API endpoints is available using `/api/help`.

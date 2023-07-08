# api

The WebRTC signaling is implemented through HTTP requests:

# get Ice configuration  
 - /api/getIceServers :  get IceServers configuration used by streamer side 

 # initiatiate communication as a caller
 - /api/call          : send offer and get answer
 - /api/whep          : similar to /api/call using [WHEP](https://www.ietf.org/archive/id/draft-murillo-whep-02.txt)
# initiatiate communication asking to be called 
 - /api/createOffer   : create an offer 
 - /api/setAnswer     : set an answer

# call communication  
 - /api/hangup        : close a call

# Ice notifications  
 - /api/addIceCandidate : add a candidate
 - /api/getIceCandidate : get the list of candidates

The list of HTTP API is available using /api/help.

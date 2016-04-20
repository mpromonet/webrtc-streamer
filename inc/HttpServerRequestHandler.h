/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerHandler.h
** 
** -------------------------------------------------------------------------*/

#include "webrtc/base/httpserver.h"

#include "PeerConnectionManager.h"

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
class HttpServerRequestHandler : public sigslot::has_slots<> 
{
	public:
		HttpServerRequestHandler(rtc::HttpServer* server, PeerConnectionManager* webRtcServer) : m_server(server), m_webRtcServer(webRtcServer)
		{
			m_server->SignalHttpRequest.connect(this, &HttpServerRequestHandler::OnRequest);
		}

		void OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t);
		
	protected:
		rtc::HttpServer*       m_server;
		PeerConnectionManager* m_webRtcServer;
};

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
		HttpServerRequestHandler(rtc::HttpServer* server, PeerConnectionManager* webRtcServer, const char* webroot) 
			: m_server(server), m_webRtcServer(webRtcServer), m_webroot(webroot)
		{
			// add a trailing '/'
			if ((m_webroot.begin() != m_webroot.end()) && *m_webroot.end() != '/')
			{
				m_webroot.push_back('/');
			}
			m_server->SignalHttpRequest.connect(this, &HttpServerRequestHandler::OnRequest);
		}

		void OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t);
		
	protected:
		rtc::HttpServer*       m_server;
		PeerConnectionManager* m_webRtcServer;
		std::string            m_webroot;
};

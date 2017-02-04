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

typedef std::function<Json::Value(const rtc::Url<char>& , const Json::Value &)> httpFunction;

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
class HttpServerRequestHandler : public sigslot::has_slots<> 
{
	public:
		HttpServerRequestHandler(rtc::HttpServer* server, PeerConnectionManager* webRtcServer, const char* webroot); 
	
		void OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t);
			
	protected:
		rtc::HttpServer*       m_server;
		PeerConnectionManager* m_webRtcServer;
		std::string            m_webroot;
		std::map<std::string,httpFunction> m_func;
};

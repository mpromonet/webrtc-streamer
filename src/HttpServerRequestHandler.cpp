/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerHandler.cpp
** 
** -------------------------------------------------------------------------*/

#include <iostream>

#include "webrtc/base/pathutils.h"
#include "webrtc/base/httpcommon-inl.h"

#include "HttpServerRequestHandler.h"

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(rtc::HttpServer* server, PeerConnectionManager* webRtcServer, const char* webroot) 
	: m_server(server), m_webRtcServer(webRtcServer), m_webroot(webroot)
{
	// add a trailing '/'
	if ((m_webroot.rbegin() != m_webroot.rend()) && (*m_webroot.rbegin() != '/'))
	{
		m_webroot.push_back('/');
	}
	m_server->SignalHttpRequest.connect(this, &HttpServerRequestHandler::OnRequest);
	
	m_func["/getDeviceList"]         = std::bind(&HttpServerRequestHandler::getDeviceList        , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/getIceServers"]         = std::bind(&HttpServerRequestHandler::getIceServers        , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/call"]                  = std::bind(&HttpServerRequestHandler::call                 , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/hangup"]                = std::bind(&HttpServerRequestHandler::hangup               , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/getIceCandidate"]       = std::bind(&HttpServerRequestHandler::getIceCandidate      , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/addIceCandidate"]       = std::bind(&HttpServerRequestHandler::addIceCandidate      , this, std::placeholders::_1, std::placeholders::_2);
	m_func["/getPeerConnectionList"] = std::bind(&HttpServerRequestHandler::getPeerConnectionList, this, std::placeholders::_1, std::placeholders::_2);
	m_func["/help"]                  = std::bind(&HttpServerRequestHandler::help                 , this, std::placeholders::_1, std::placeholders::_2);
}

Json::Value HttpServerRequestHandler::getDeviceList(const rtc::Url<char>& url, const Json::Value & in) {
	return m_webRtcServer->getDeviceList();
}

Json::Value HttpServerRequestHandler::getIceServers(const rtc::Url<char>& url, const Json::Value & in) {
	return m_webRtcServer->getIceServers();
}

Json::Value HttpServerRequestHandler::call(const rtc::Url<char>& url, const Json::Value & in) {
	std::string peerid;
	url.get_attribute("peerid",&peerid);
	return m_webRtcServer->call(peerid, in);
}

Json::Value HttpServerRequestHandler::hangup(const rtc::Url<char>& url, const Json::Value & in) {
	std::string peerid;
	url.get_attribute("peerid",&peerid);
	m_webRtcServer->hangUp(peerid);
	Json::Value answer(1);
	return answer;
}

Json::Value HttpServerRequestHandler::getIceCandidate(const rtc::Url<char>& url, const Json::Value & in) {
	std::string peerid;
	url.get_attribute("peerid",&peerid);
	return m_webRtcServer->getIceCandidateList(peerid);
}

Json::Value HttpServerRequestHandler::addIceCandidate(const rtc::Url<char>& url, const Json::Value & in) {
	std::string peerid;
	url.get_attribute("peerid",&peerid);
	m_webRtcServer->addIceCandidate(peerid, in);
	Json::Value answer(1);
	return answer;
}

Json::Value HttpServerRequestHandler::getPeerConnectionList(const rtc::Url<char>& url, const Json::Value & in) {
	return m_webRtcServer->getPeerConnectionList();
}

Json::Value HttpServerRequestHandler::help(const rtc::Url<char>& url, const Json::Value & in) {
	Json::Value answer;
	for (auto it : m_func) 
	{
		answer.append(it.first);
	}
	return answer;
}

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
void HttpServerRequestHandler::OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t) 
{
	// parse URL
	rtc::Url<char> url(t->request.path,"");	
	std::cout << "===> HTTP request path:" <<  url.path() << std::endl;

	// Allow CORS
	t->response.addHeader("Access-Control-Allow-Origin","*");
	
	// read body
	size_t size = 0;
	t->request.document->GetSize(&size);
	t->request.document->Rewind();
	char buffer[size];
	size_t readsize = 0;
	rtc::StreamResult res = t->request.document->ReadAll(&buffer, size, &readsize, NULL);
	
	if (res == rtc::SR_SUCCESS)
	{
		std::map<std::string,httpFunction>::iterator it = m_func.find(url.path());
		if (it != m_func.end())
		{
			Json::Reader reader;
			Json::Value  jmessage;
			std::string body(buffer, readsize);	
			std::cout << "body:" << body << std::endl;
			
			if (!reader.parse(body, jmessage)) 
			{
				LOG(WARNING) << "Received unknown message:" << body;
			}
			Json::Value out(it->second(url, jmessage));
			if (out.isNull() == false)
			{
				std::string answer(Json::StyledWriter().write(out));
				rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
				t->response.set_success("text/plain", mem);			
			}			
		}
		else
		{
			std::string path(url.path());
			// remove "/"
			path = basename(path.c_str());
			if (path.empty())
			{
				path = "index.html";
			}
			path.insert(0, m_webroot);
			std::cout << "filename:" << path << std::endl;
			rtc::Pathname pathname(path);
			rtc::FileStream* fs = rtc::Filesystem::OpenFile(pathname, "rb");
			if (fs)
			{
				t->response.set_success("text/html", fs);			
			}
		}
	}
	
	t->response.setHeader(rtc::HH_CONNECTION,"Close");
	m_server->Respond(t);
}

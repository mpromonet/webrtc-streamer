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
	if ((m_webroot.rbegin() != m_webroot.rend()) && (*m_webroot.rbegin() != '/')) {
		m_webroot.push_back('/');
	}
	// connect signal to slot
	m_server->SignalHttpRequest.connect(this, &HttpServerRequestHandler::OnRequest);
	
	// http api callbacks
	m_func["/getDeviceList"]         = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		return m_webRtcServer->getDeviceList();
	};
	
	m_func["/getIceServers"]         = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		return m_webRtcServer->getIceServers();
	};
	
	m_func["/call"]                  = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		std::string connecturl;
		url.get_attribute("url",&connecturl);
		return m_webRtcServer->call(peerid, connecturl, in);
	};
	
	m_func["/hangup"]                = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		Json::Value answer(m_webRtcServer->hangUp(peerid));
		return answer;
	};
	
	m_func["/createOffer"]           = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		std::string connecturl;
		url.get_attribute("url",&connecturl);
		return m_webRtcServer->createOffer(peerid, connecturl);
	};
	m_func["/setAnswer"]             = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		m_webRtcServer->setAnswer(peerid, in);
		Json::Value answer(1);
		return answer;
	};
	
	m_func["/getIceCandidate"]       = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		return m_webRtcServer->getIceCandidateList(peerid);
	};
	
	m_func["/addIceCandidate"]       = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string peerid;
		url.get_attribute("peerid",&peerid);
		m_webRtcServer->addIceCandidate(peerid, in);
		Json::Value answer(1);
		return answer;
	};
	
	m_func["/getPeerConnectionList"] = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		return m_webRtcServer->getPeerConnectionList();
	};

	m_func["/getStreamList"] = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		return m_webRtcServer->getStreamList();
	};

	m_func["/delStream"] = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		std::string connecturl;
		url.get_attribute("url",&connecturl);
		Json::Value answer(m_webRtcServer->delStream(connecturl));
		return answer;		
	};
	
	m_func["/help"]                  = [this](const rtc::Url<char>& url, const Json::Value & in) -> Json::Value { 
		Json::Value answer;
		for (auto it : m_func) {
			answer.append(it.first);
		}
		return answer;
	};
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
			std::string body(buffer, readsize);	
			std::cout << "body:" << body << std::endl;

			// parse in
			Json::Reader reader;
			Json::Value  jmessage;			
			if (!reader.parse(body, jmessage)) 
			{
				LOG(WARNING) << "Received unknown message:" << body;
			}
			// invoke API implementation
			Json::Value out(it->second(url, jmessage));
			
			// fill out
			if (out.isNull() == false)
			{
				std::string answer(Json::StyledWriter().write(out));
				std::cout << "answer:" << answer << std::endl;
				
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

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
**  http callback
** -------------------------------------------------------------------------*/
void HttpServerRequestHandler::OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t) 
{
	std::cout << "===> HTTP request " <<  t->request.path << std::endl;
	
	// parse URL
	rtc::Url<char> url(t->request.path,"");	
	std::cout << "===> HTTP request path:" <<  url.path() << std::endl;
	std::string peerid;
	url.get_attribute("peerid",&peerid);
	std::cout << "===> HTTP request PEERID:" << peerid << std::endl;

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
		std::string body(buffer, readsize);			
		std::cout << "body:" << body << std::endl;
			
		if (url.path() == "/getDeviceList")
		{
			Json::Value jsonAnswer(m_webRtcServer->getDeviceList());
			
			std::string answer(Json::StyledWriter().write(jsonAnswer));			
			rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
			t->response.set_success("text/plain", mem);			
		}
		else if (url.path() == "/getIceServers")
		{
			Json::Value jsonAnswer(m_webRtcServer->getIceServers());
			
			std::string answer(Json::StyledWriter().write(jsonAnswer));			
			rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
			t->response.set_success("text/plain", mem);			
		}
		else if (url.path() == "/call")
		{
			Json::Reader reader;
			Json::Value  jmessage;
			
			if (!reader.parse(body, jmessage)) 
			{
				LOG(WARNING) << "Received unknown message:" << body;
			}
			else
			{
				Json::Value jsonAnswer(m_webRtcServer->call(jmessage));
				
				if (jsonAnswer.isNull() == false)
				{
					std::string answer(Json::StyledWriter().write(jsonAnswer));
					rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
					t->response.set_success("text/plain", mem);			
				}
			}
		}
		else if (url.path() == "/hangup")
		{
			m_webRtcServer->hangUp(peerid);
			t->response.set_success();			
		}
		else if (url.path() == "/getIceCandidate")
		{		
			Json::Value jsonAnswer(m_webRtcServer->getIceCandidateList(peerid));
			
			std::string answer(Json::StyledWriter().write(jsonAnswer));				
			rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
			t->response.set_success("text/plain", mem);			
		}
		else if (url.path() == "/addIceCandidate")
		{
			Json::Reader reader;
			Json::Value  jmessage;
			
			if (!reader.parse(body, jmessage)) 
			{
				LOG(WARNING) << "Received unknown message:" << body;
			}
			else
			{
				m_webRtcServer->addIceCandidate(peerid, jmessage);			
				t->response.set_success();			
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

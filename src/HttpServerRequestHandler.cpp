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

#include "HttpServerRequestHandler.h"

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
void HttpServerRequestHandler::OnRequest(rtc::HttpServer*, rtc::HttpServerTransaction* t) 
{
	std::string host;
	std::string path;
	t->request.getRelativeUri(&host, &path);
	std::cout << "===> HTTP request " <<  path << std::endl;
	
	size_t size = 0;
	t->request.document->GetSize(&size);
	t->request.document->Rewind();
	char buffer[size];
	size_t readsize = 0;
	rtc::StreamResult res = t->request.document->ReadAll(&buffer, size, &readsize, NULL);
	std::cout << "res:" << res << std::endl;
	std::string body(buffer, readsize);			
	std::cout << "body:" << body << std::endl;
	
	if (path == "/getDeviceList")
	{
		std::string answer(Json::StyledWriter().write(m_webRtcServer->getDeviceList()));
		
		rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
		t->response.set_success("text/plain", mem);			
	}
	else if (path == "/offer")
	{
		std::string peerid;					
		std::string answer(m_webRtcServer->getOffer(peerid,body));
		std::cout << peerid << ":" << answer << std::endl;
		
		if (answer.empty() == false)
		{
			rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
			t->response.addHeader("peerid",peerid);	
			t->response.set_success("text/plain", mem);			
		}
	}
	else if (path == "/answer")
	{
		std::string peerid;	
		t-> request.hasHeader("peerid", &peerid);				
		m_webRtcServer->setAnswer(peerid, body);
		
		t->response.set_success();			
	}
	else if (path == "/getIceCandidate")
	{
		std::string peerid;	
		t->request.hasHeader("peerid", &peerid);				
		
		std::string answer(Json::StyledWriter().write(m_webRtcServer->getIceCandidateList(peerid)));					
		rtc::MemoryStream* mem = new rtc::MemoryStream(answer.c_str(), answer.size());			
		t->response.set_success("text/plain", mem);			
	}
	else if (path == "/addIceCandidate")
	{
		std::string peerid;	
		t-> request.hasHeader("peerid", &peerid);				
		m_webRtcServer->addIceCandidate(peerid, body);	
		
		t->response.set_success();			
	}
	else
	{
		rtc::Pathname pathname("index.html");
		rtc::FileStream* fs = rtc::Filesystem::OpenFile(pathname, "rb");
		if (fs)
		{
			t->response.set_success("text/html", fs);			
		}
	}
	t->response.setHeader(rtc::HH_CONNECTION,"Close");
	m_server->Respond(t);
}

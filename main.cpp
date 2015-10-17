/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <map>
#include <list>

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/stunserver.h"

#include "mongoose.h"

#include "PeerConnectionManager.h"

typedef int (*callback)(struct mg_connection *conn);
std::map<std::string, callback> m_urlmap;

#define URL_CALLBACK(uri, arg) \
int handle_##uri (struct mg_connection *arg); \
std::pair<std::map<std::string, callback>::iterator,bool> m_urlmap ## uri = m_urlmap.insert(std::pair<std::string, callback>("/"#uri,handle_##uri)); \
int handle_##uri(struct mg_connection *arg) \
/* */

URL_CALLBACK(getDeviceList, conn)
{	
	PeerConnectionManager* webRtcServer =(PeerConnectionManager*)conn->server_param;	
	Json::Value list(webRtcServer->getDeviceList());
	Json::StyledWriter writer;
	std::string msg(writer.write(list));
	mg_send_data(conn, msg.c_str(), msg.size());
	return MG_TRUE;
}

URL_CALLBACK(offer, conn)
{	
	PeerConnectionManager* webRtcServer =(PeerConnectionManager*)conn->server_param;
	char buffer[1024];
	if (mg_get_var(conn, "url", buffer, sizeof(buffer)) > 0)
	{
		std::string peerid;	
		std::string msg(webRtcServer->getOffer(peerid,buffer));
		mg_send_header(conn, "peerid", peerid.c_str());
		mg_send_data(conn, msg.c_str(), msg.size());
	}
	return MG_TRUE;
}

URL_CALLBACK(answer, conn)
{	
	PeerConnectionManager* webRtcServer =(PeerConnectionManager*)conn->server_param;	
	std::string answer(conn->content,conn->content_len);
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	webRtcServer->setAnswer(peerid, answer);
	mg_send_header(conn, "peerid", peerid.c_str());
	return MG_TRUE;
}

URL_CALLBACK(candidate, conn)
{	
	PeerConnectionManager* webRtcServer =(PeerConnectionManager*)conn->server_param;	
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	Json::Value list(webRtcServer->getIceCandidateList(peerid));
	Json::StyledWriter writer;
	std::string msg(writer.write(list));
	mg_send_data(conn, msg.c_str(), msg.size());
	return MG_TRUE;
}

URL_CALLBACK(addicecandidate, conn)
{	
	PeerConnectionManager* webRtcServer =(PeerConnectionManager*)conn->server_param;	
	std::string answer(conn->content,conn->content_len);
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	webRtcServer->addIceCandidate(peerid, answer);
	mg_send_header(conn, "peerid", peerid.c_str());
	return MG_TRUE;
}

/* ---------------------------------------------------------------------------
**  mongoose callback
** -------------------------------------------------------------------------*/
static int ev_handler(struct mg_connection *conn, enum mg_event ev) 
{
	int ret = MG_FALSE;
	switch (ev) 
	{
		case MG_AUTH: ret = MG_TRUE; break;
		case MG_REQUEST: 
		{
			if (conn->uri)
			{
				std::map<std::string, callback>::iterator it = m_urlmap.find(conn->uri);
				if (it != m_urlmap.end())
				{
					ret = it->second(conn);
				}
			}
		}
		break;
		default: break;
	} 
	return ret;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) {
	const char* port     = "8080";
	const char* stunurl  = "127.0.0.1:3478";
	int logLevel = rtc::LERROR; 
	
	int c = 0;     
	while ((c = getopt (argc, argv, "hS:P:v::")) != -1)
	{
		switch (c)
		{
			case 'v': logLevel--; if (optarg) logLevel-=strlen(optarg); break;
			case 'P': port = optarg; break;
			case 'S': stunurl = optarg; break;
			case 'h':
			default:
				std::cout << argv[0] << " [-P http port] [-S stun address] -[v[v]]"                             << std::endl;
				std::cout << "\t -v[v[v]]         : verbosity"                                                  << std::endl;
				std::cout << "\t -P port          : HTTP port (default "               << port    << ")"        << std::endl;
				std::cout << "\t -S stun_address  : STUN listenning address (default " << stunurl << ")"        << std::endl;
				exit(0);
		}
	}
	
	rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetMinLogSeverity() << std::endl; 
	
	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();
	
	// webrtc server
	PeerConnectionManager webRtcServer(stunurl);
	if (!webRtcServer.InitializePeerConnection())
	{
		std::cout << "Cannot Initialize WebRTC server" << std::endl; 
	}
	else
	{	
		// http server
		struct mg_server *server = mg_create_server(&webRtcServer, ev_handler);
		mg_set_option(server, "listening_port", port);
		std::string currentPath(get_current_dir_name());
		mg_set_option(server, "document_root", currentPath.c_str());
		std::cout << "Started on port:" << mg_get_option(server, "listening_port") << " webroot:" << mg_get_option(server, "document_root") << std::endl; 
		
		// STUN server
		rtc::SocketAddress server_addr;
		server_addr.FromString(stunurl);
		cricket::StunServer* stunserver = NULL;
		rtc::AsyncUDPSocket* server_socket = rtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
		if (server_socket) 
		{
			stunserver = new cricket::StunServer(server_socket);
			std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
		}
	  
		// mainloop
		while(thread->ProcessMessages(10))
		{
			 mg_poll_server(server, 10);
		}
	}
	
	rtc::CleanupSSL();
	return 0;
}


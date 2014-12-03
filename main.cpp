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
#include <tuple>
#include <list>

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/stunserver.h"

#include "mongoose.h"

#include "webrtc.h"

typedef int (*callback)(struct mg_connection *conn);
std::map<std::string, callback> m_urlmap;

#define URL_CALLBACK(uri, fct, arg) \
int fct(struct mg_connection *arg); \
std::pair<std::map<std::string, callback>::iterator,bool> m_urlmap ## fct = m_urlmap.insert(std::pair<std::string, callback>(uri,fct)); \
int fct(struct mg_connection *arg) \
/* */

URL_CALLBACK("/offer", handle_offer, conn)
{	
	PeerConnectionManager* conductor =(PeerConnectionManager*)conn->server_param;
	std::string peerid;	
	std::string msg(conductor->getOffer(peerid));
	mg_send_header(conn, "peerid", peerid.c_str());
	mg_send_data(conn, msg.c_str(), msg.size());
	return MG_TRUE;
}

URL_CALLBACK("/answer", handle_answer, conn)
{	
	PeerConnectionManager* conductor =(PeerConnectionManager*)conn->server_param;	
	std::string answer(conn->content,conn->content_len);
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	conductor->setAnswer(peerid, answer);
	mg_send_header(conn, "peerid", peerid.c_str());
	return MG_TRUE;
}

URL_CALLBACK("/candidate", handle_candidate, conn)
{	
	PeerConnectionManager* conductor =(PeerConnectionManager*)conn->server_param;	
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	Json::Value list(conductor->getIceCandidateList(peerid));
	Json::StyledWriter writer;
	std::string msg(writer.write(list));
	mg_send_data(conn, msg.c_str(), msg.size());
	return MG_TRUE;
}

URL_CALLBACK("/addicecandidate", handle_addicecandidate, conn)
{	
	PeerConnectionManager* conductor =(PeerConnectionManager*)conn->server_param;	
	std::string answer(conn->content,conn->content_len);
	std::string peerid;
	const char * peeridheader = mg_get_header(conn, "peerid");
	if (peeridheader) peerid = peeridheader;
	conductor->addIceCandidate(peerid, answer);
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
	const char* device   = "/dev/video0";
	const char* stunurl  = "127.0.0.1:3478";
	int logLevel = rtc::LERROR; 
	
	int c = 0;     
	while ((c = getopt (argc, argv, "hP:v::")) != -1)
	{
		switch (c)
		{
			case 'v': logLevel--; if (optarg) logLevel-=strlen(optarg); break;
			case 'P': port = optarg; break;
			case 'h':
			default:
				std::cout << argv[0] << " [-P http port] [device]"                    << std::endl;
				std::cout << "\t -v[v[v]] : verbosity"                                << std::endl;
				std::cout << "\t -P port  : HTTP port (default "<< port << ")"        << std::endl;
				std::cout << "\t device   : capture device (default "<< device << ")" << std::endl;
				exit(0);
		}
	}
	if (optind<argc)
	{
		device = argv[optind];
	}	
	
	rtc::LogMessage::LogToDebug(logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetMinLogSeverity() << std::endl; 
	
	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();
	
	// webrtc server
	PeerConnectionManager conductor(device, stunurl);

	// http server
	struct mg_server *server = mg_create_server(&conductor, ev_handler);
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
	
	rtc::CleanupSSL();
	return 0;
}


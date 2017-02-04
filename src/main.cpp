/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** 
** -------------------------------------------------------------------------*/

#include <iostream>

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/stunserver.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{
	const char* localstunurl  = "127.0.0.1:3478";
	const char* stunurl       = "stun.l.google.com:19302";
	int logLevel              = rtc::LERROR; 
	const char* webroot       = "./html";
	
	std::string defaultAddress("0.0.0.0:");
	int defaultPort = 8000;
	const char * port = getenv("PORT");
	if (port) 
	{
		defaultPort = atoi(port);
	} 
	defaultAddress.append(std::to_string(defaultPort));
	
	int c = 0;     
	while ((c = getopt (argc, argv, "hH:v::w:" "S:s::")) != -1)
	{
		switch (c)
		{
			case 'v': logLevel--; if (optarg) logLevel-=strlen(optarg); break;
			case 'H': defaultAddress = optarg; break;
			case 'w': webroot = optarg; break;
			
			case 'S': localstunurl = optarg; stunurl = localstunurl; break;
			case 's': localstunurl = NULL; if (optarg) stunurl = optarg; break;
			
			case 'h':
			default:
				std::cout << argv[0] << " [-H http port] [-S embeded stun address] -[v[v]]  [url1]...[urln]"                      << std::endl;
				std::cout << argv[0] << " [-H http port] [-s externel stun address] -[v[v]] [url1]...[urln]"                      << std::endl;
				std::cout << "\t -v[v[v]]           : verbosity"                                                                  << std::endl;
				std::cout << "\t -H hostname:port   : HTTP server binding (default "   << defaultAddress    << ")"                << std::endl;
				std::cout << "\t -w webroot         : path to get files"                                                                  << std::endl;
				std::cout << "\t -S stun_address    : start embeded STUN server bind to address (default " << localstunurl << ")" << std::endl;
				std::cout << "\t -s[stun_address]   : use an external STUN server (default " << stunurl << ")"                    << std::endl;
				std::cout << "\t [url]              : url to register in the source list"                                         << std::endl;
				exit(0);
		}
	}

	std::list<std::string> urlList;
	while (optind<argc)
	{
		urlList.push_back(argv[optind]);
		optind++;
	}	
	
	rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetMinLogSeverity() << std::endl; 
	
	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();

	// webrtc server
	PeerConnectionManager webRtcServer(stunurl, urlList);
	if (!webRtcServer.InitializePeerConnection())
	{
		std::cout << "Cannot Initialize WebRTC server" << std::endl; 
	}
	else
	{
		// http server
		rtc::HttpListenServer httpServer;
		rtc::SocketAddress http_addr;
		http_addr.FromString(defaultAddress);
		std::cout << "HTTP Listen at " << http_addr.ToString() << std::endl;
		int ret = httpServer.Listen(http_addr);
		if (ret != 0)
		{
			std::cout << "Cannot Initialize start HTTP server " << strerror(ret) << std::endl; 
		}
		else
		{
			// connect httpserver to a request handler
			HttpServerRequestHandler http(&httpServer, &webRtcServer, webroot);
			std::cout << "HTTP Listening at " << http_addr.ToString() << std::endl;

			// start STUN server if needed
			std::unique_ptr<cricket::StunServer> stunserver;
			if (localstunurl != NULL)
			{
				rtc::SocketAddress server_addr;
				server_addr.FromString(localstunurl);
				rtc::AsyncUDPSocket* server_socket = rtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
				if (server_socket) 
				{
					stunserver.reset(new cricket::StunServer(server_socket));
					std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
				}	
			}			

			// mainloop
			thread->Run();
		}
	}
	
	rtc::CleanupSSL();
	return 0;
}


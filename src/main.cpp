/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include <signal.h>

#include <iostream>
#include <fstream>

#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "p2p/base/stun_server.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/turn_server.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"

#if WIN32
#include "getopt.h"
#endif

void sighandler(int n)
{
        printf("SIGINT\n");
        rtc::Thread* thread = rtc::Thread::Current();
	thread->Stop(); 
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	const char* turnurl       = "";
	const char* defaultlocalstunurl  = "0.0.0.0:3478";
	const char* localstunurl  = NULL;
	const char* defaultlocalturnurl  = "turn:turn@0.0.0.0:3478";
	const char* localturnurl  = NULL;
	const char* stunurl       = "stun.l.google.com:19302";
	int logLevel              = rtc::LERROR;
	const char* webroot       = "./html";
	std::string sslCertificate;
	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kPlatformDefaultAudio;
	std::string streamName;
	std::string nbthreads;
	std::string passwdFile;
	std::string authDomain = "mydomain.com";
	std::string publishFilter(".*");
	Json::Value config;  

	std::string httpAddress("0.0.0.0:");
	std::string httpPort = "8000";
	const char * port = getenv("PORT");
	if (port)
	{
		httpPort = port;
	}
	httpAddress.append(httpPort);

	int c = 0;
	while ((c = getopt (argc, argv, "hVv::" "c:H:w:N:A:D:C:" "T::t:S::s::" "a::q:" "n:u:U:")) != -1)
	{
		switch (c)
		{
			case 'H': httpAddress = optarg; break;
			case 'c': sslCertificate = optarg; break;
			case 'w': webroot = optarg; break;
			case 'N': nbthreads = optarg; break;
			case 'A': passwdFile = optarg; break;
			case 'D': authDomain = optarg; break;

			case 'T': localturnurl = optarg ? optarg : defaultlocalturnurl; turnurl = localturnurl; break;
			case 't': turnurl = optarg; break;
			case 'S': localstunurl = optarg ? optarg : defaultlocalstunurl; stunurl = localstunurl; break;
			case 's': stunurl = optarg ? optarg : defaultlocalstunurl; break;
			
			case 'a': audioLayer = optarg ? (webrtc::AudioDeviceModule::AudioLayer)atoi(optarg) : webrtc::AudioDeviceModule::kDummyAudio; break;
			case 'q': publishFilter = optarg ; break;
				
			case 'C': {
				std::ifstream stream(optarg);
				stream >> config;
				break;
			}

			case 'n': streamName = optarg; break;
			case 'u': {
				if (!streamName.empty()) {
					config["urls"][streamName]["video"] = optarg; 
				}
			}
			break;
			case 'U': {
				if (!streamName.empty()) {
					config["urls"][streamName]["audio"] = optarg; 
				}
			}
			break;
			
			case 'v': 
				logLevel--; 
				if (optarg) {
					logLevel-=strlen(optarg); 
				}
			break;			
			case 'V':
				std::cout << VERSION << std::endl;
				exit(0);
			break;
			case 'h':
			default:
				std::cout << argv[0] << " [-H http port] [-S[embeded stun address]] [-t [username:password@]turn_address] -[v[v]]  [url1]...[urln]" << std::endl;
				std::cout << argv[0] << " [-H http port] [-s[externel stun address]] [-t [username:password@]turn_address] -[v[v]] [url1]...[urln]" << std::endl;
				std::cout << argv[0] << " -V" << std::endl;

				std::cout << "\t -v[v[v]]           : verbosity"                                                                  << std::endl;
				std::cout << "\t -V                 : print version"                                                              << std::endl;

				std::cout << "\t -H hostname:port   : HTTP server binding (default "   << httpAddress    << ")"                   << std::endl;
				std::cout << "\t -w webroot         : path to get files"                                                          << std::endl;
				std::cout << "\t -c sslkeycert      : path to private key and certificate for HTTPS"                              << std::endl;
				std::cout << "\t -N nbthreads       : number of threads for HTTP server"                                          << std::endl;
				std::cout << "\t -A passwd          : password file for HTTP server access"                                          << std::endl;
				std::cout << "\t -D authDomain      : authentication domain for HTTP server access (default:mydomain.com)"                                       << std::endl;
			
				std::cout << "\t -S[stun_address]                   : start embeded STUN server bind to address (default " << defaultlocalstunurl << ")" << std::endl;
				std::cout << "\t -s[stun_address]                   : use an external STUN server (default:" << stunurl << " , -:means no STUN)"                    << std::endl;
				std::cout << "\t -t[username:password@]turn_address : use an external TURN relay server (default:disabled)"       << std::endl;
				std::cout << "\t -T[username:password@]turn_address : start embeded TURN server (default:disabled)"       << std::endl;

				std::cout << "\t -a[audio layer]                    : spefify audio capture layer to use (default:" << audioLayer << ")"          << std::endl;

				std::cout << "\t -n name -u videourl -U audiourl    : register a stream with name using url"                         << std::endl;			
				std::cout << "\t [url]                              : url to register in the source list"                                         << std::endl;
				std::cout << "\t -C config.json                     : load urls from JSON config file"                                                 << std::endl;
			
				exit(0);
		}
	}

	while (optind<argc)
	{
		std::string url(argv[optind]);
		config["urls"][url]["video"] = url; 
		optind++;
	}

	std::cout  << "Version:" << VERSION << std::endl;

	std::cout  << config;

	rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetLogToDebug() << std::endl;

	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();

	// webrtc server
	std::list<std::string> iceServerList;
	if ((strlen(stunurl) != 0) && (strcmp(stunurl,"-") != 0)) {
		iceServerList.push_back(std::string("stun:")+stunurl);
	}
	if (strlen(turnurl)) {
		iceServerList.push_back(std::string("turn:")+turnurl);
	}

	PeerConnectionManager webRtcServer(iceServerList, config["urls"], audioLayer, publishFilter);
	if (!webRtcServer.InitializePeerConnection())
	{
		std::cout << "Cannot Initialize WebRTC server" << std::endl;
	}
	else
	{
		// http server
		std::vector<std::string> options;
		options.push_back("document_root");
		options.push_back(webroot);
		options.push_back("enable_directory_listing");
		options.push_back("no");
		options.push_back("additional_header");
		options.push_back("X-Frame-Options: SAMEORIGIN");
		options.push_back("access_control_allow_origin");
		options.push_back("*");
		options.push_back("listening_ports");
		options.push_back(httpAddress);
		options.push_back("enable_keep_alive");
		options.push_back("yes");
		options.push_back("keep_alive_timeout_ms");
		options.push_back("1000");
		if (!sslCertificate.empty()) {
			options.push_back("ssl_certificate");
			options.push_back(sslCertificate);
		}
		if (!nbthreads.empty()) {
			options.push_back("num_threads");
			options.push_back(nbthreads);
		}
		if (!passwdFile.empty()) {
			options.push_back("global_auth_file");
			options.push_back(passwdFile);
			options.push_back("authentication_domain");
			options.push_back(authDomain);
		}
		
		try {
			std::map<std::string,HttpServerRequestHandler::httpFunction> func = webRtcServer.getHttpApi();
			std::cout << "HTTP Listen at " << httpAddress << std::endl;
			HttpServerRequestHandler httpServer(func, options);

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

			// start TRUN server if needed
			std::unique_ptr<cricket::TurnServer> turnserver;
			if (localturnurl != NULL)
			{
				std::istringstream is(localturnurl);
				std::string addr;
				std::getline(is, addr, '@');
				std::getline(is, addr, '@');
				rtc::SocketAddress server_addr;
				server_addr.FromString(addr);
				rtc::AsyncUDPSocket* server_socket = rtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
				if (server_socket)
				{
					turnserver.reset(new cricket::TurnServer(rtc::Thread::Current()));
					std::cout << "TURN Listening at " << server_addr.ToString() << std::endl;
					turnserver->AddInternalSocket(server_socket, cricket::PROTO_UDP);
					turnserver->SetExternalSocketFactory(new rtc::BasicPacketSocketFactory(), rtc::SocketAddress(server_addr.ipaddr(), 0));
				}
			}
			
			// mainloop
			signal(SIGINT,sighandler);
			thread->Run();

		} catch (const CivetException & ex) {
			std::cout << "Cannot Initialize start HTTP server exception:" << ex.what() << std::endl;
		}
	}

	rtc::CleanupSSL();
	std::cout << "Exit" << std::endl;
	return 0;
}


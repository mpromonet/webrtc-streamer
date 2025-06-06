/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include <signal.h>

#ifndef _WIN32
#include <libgen.h> 
#endif

#include <iostream>
#include <fstream>

#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "p2p/test/stun_server.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/test/turn_server.h"

#include "system_wrappers/include/field_trial.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"

#if WIN32
#include "getopt.h"
#endif

PeerConnectionManager* webRtcServer = NULL;

void sighandler(int n)
{
	printf("SIGINT\n");
	// delete need thread still running
	delete webRtcServer;
	webRtcServer = NULL;
	webrtc::Thread::Current()->Quit(); 
}

class TurnAuth : public webrtc::TurnAuthInterface {
	public:
		virtual bool GetKey(absl::string_view username,absl::string_view realm, std::string* key) { 
			return webrtc::ComputeStunCredentialHash(std::string(username), std::string(realm), std::string(username), key); 
		}
};

class TurnRedirector : public webrtc::TurnRedirectInterface
{
public:
	explicit TurnRedirector() {}

	virtual bool ShouldRedirect(const webrtc::SocketAddress &, webrtc::SocketAddress *out)
	{
		return true;
	}
};

std::string GetExeDir(const char* argv0) {
	std::string exeDir;
#ifdef _WIN32
	char resolvedPath[MAX_PATH];
	if (GetModuleFileNameA(NULL, resolvedPath, MAX_PATH)) {
		std::string fullPath(resolvedPath);
		size_t pos = fullPath.find_last_of("\\/");
		if (pos != std::string::npos) {
			exeDir = fullPath.substr(0, pos + 1);
		}
	}
#else
	char resolvedPath[PATH_MAX];
    if (realpath(argv0, resolvedPath)) {
        std::string fullPath(resolvedPath);
        exeDir = dirname(resolvedPath);
		exeDir += "/";
    }
#endif	
	return exeDir;
}


std::string GetDefaultRessourceDir(const char* argv0) {
   
	std::string ressourceDir(WEBRTCSTREAMERRESSOURCE);
	if (ressourceDir[0] != '/') {
		std::string exeDir = GetExeDir(argv0);
		ressourceDir = exeDir + ressourceDir;
	}
	
    return ressourceDir;
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
	std::string localWebrtcUdpPortRange = "0:65535";
	int logLevel              = webrtc::LS_NONE;
	std::string webroot       = GetDefaultRessourceDir(argv[0]);
	std::string basePath;
	std::string sslCertificate;
	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kPlatformDefaultAudio;
	std::string nbthreads;
	std::string passwdFile;
	std::string authDomain = "mydomain.com";
	bool        disableXframeOptions = false;

	std::string publishFilter(".*");
	Json::Value config;  
	bool        useNullCodec = false;
	bool        usePlanB = false;
	int         maxpc = 0;
	webrtc::PeerConnectionInterface::IceTransportsType transportType = webrtc::PeerConnectionInterface::IceTransportsType::kAll;
	std::string webrtcTrialsFields = "WebRTC-FrameDropper/Disabled/WebRTC-Video-H26xPacketBuffer/Enabled/";
	TurnAuth 	turnAuth;
	TurnRedirector turnRedirector;

	std::string httpAddress("0.0.0.0:");
	std::string httpPort = "8000";
	const char * port = getenv("PORT");
	if (port)
	{
		httpPort = port;
	}
	httpAddress.append(httpPort);

	std::string streamName;
	int c = 0;
	while ((c = getopt (argc, argv, "hVv::C:" "c:H:w:N:A:D:XB:" "m:I:" "T::t:S::s::R:W:" "a::q:ob" "n:u:U:")) != -1)
	{
		switch (c)
		{
			case 'H': httpAddress = optarg;        break;
			case 'c': sslCertificate = optarg;     break;
			case 'w': webroot = optarg;            break;
			case 'N': nbthreads = optarg;          break;
			case 'A': passwdFile = optarg;         break;
			case 'D': authDomain = optarg;         break;
			case 'X': disableXframeOptions = true; break;
			case 'B': basePath = optarg;           break;

			case 'm': maxpc = atoi(optarg);        break;
			case 'I': transportType = (webrtc::PeerConnectionInterface::IceTransportsType)atoi(optarg);break;

			case 'T': localturnurl = optarg ? optarg : defaultlocalturnurl; turnurl = localturnurl; break;
			case 't': turnurl = optarg;                                                             break;
			case 'S': localstunurl = optarg ? optarg : defaultlocalstunurl; stunurl = localstunurl; break;
			case 's': stunurl = optarg ? optarg : defaultlocalstunurl;                              break;
			case 'R': localWebrtcUdpPortRange = optarg;                                             break;
			case 'W': webrtcTrialsFields = optarg;                                                  break;

			case 'a': audioLayer = optarg ? (webrtc::AudioDeviceModule::AudioLayer)atoi(optarg) : webrtc::AudioDeviceModule::kDummyAudio; break;	
			case 'q': publishFilter = optarg ; break;
			case 'o': useNullCodec = true; break;
			case 'b': usePlanB = true; break;
				
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
				std::cout << argv[0] << std::endl;

				std::cout << "  General options" << std::endl;
				std::cout << "\t -v[v[v]]                           : verbosity"                                                                        << std::endl;
				std::cout << "\t -V                                 : print version"                                                                    << std::endl;
				std::cout << "\t -C config.json                     : load urls from JSON config file"                                                  << std::endl;
				std::cout << "\t -n name -u videourl -U audiourl    : register a stream with name using url"                                            << std::endl;			
				std::cout << "\t [url]                              : url to register in the source list"                                               << std::endl;

				std::cout << std::endl << "  HTTP options" << std::endl;
				std::cout << "\t -H hostname:port                   : HTTP server binding (default "   << httpAddress    << ")"                         << std::endl;
				std::cout << "\t -w webroot                         : path to get static files (default " << webroot << ")"                             << std::endl;
				std::cout << "\t -c sslkeycert                      : path to private key and certificate for HTTPS"                                    << std::endl;
				std::cout << "\t -N nbthreads                       : number of threads for HTTP server"                                                << std::endl;
				std::cout << "\t -A passwd                          : password file for HTTP server access"                                             << std::endl;
				std::cout << "\t -D authDomain                      : authentication domain for HTTP server access (default:mydomain.com)"              << std::endl;
			
				std::cout << std::endl << "  WebRTC options" << std::endl;
				std::cout << "\t -S[stun_address]                   : start embeded STUN server bind to address (default " << defaultlocalstunurl << ")" << std::endl;
				std::cout << "\t -s[stun_address]                   : use an external STUN server (default:" << stunurl << " , -:means no STUN)"         << std::endl;
				std::cout << "\t -t[username:password@]turn_address : use an external TURN relay server (default:disabled)"                              << std::endl;
				std::cout << "\t -T[username:password@]turn_address : start embeded TURN server (default:disabled)"				                         << std::endl;
				std::cout << "\t -R Udp_port_min:Udp_port_min       : Set the webrtc udp port range (default:" << localWebrtcUdpPortRange << ")"         << std::endl;
				std::cout << "\t -W webrtc_trials_fields            : Set the webrtc trials fields (default:" << webrtcTrialsFields << ")"               << std::endl;
				std::cout << "\t -I icetransport                    : Set ice transport type (default:" << transportType << ")"                          << std::endl;
#ifdef HAVE_SOUND				
				std::cout << "\t -a[audio layer]                    : spefify audio capture layer to use (default:" << audioLayer << ")"                 << std::endl;
#endif				
				std::cout << "\t -q[filter]                         : spefify publish filter (default:" << publishFilter << ")"                          << std::endl;
				std::cout << "\t -o                                 : use null codec (keep frame encoded)"                                               << std::endl;
				std::cout << "\t -b                                 : use sdp plan-B (default use unifiedPlan)"                                          << std::endl;
			
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

	webrtc::LogMessage::LogToDebug((webrtc::LoggingSeverity)logLevel);
	webrtc::LogMessage::LogTimestamps();
	webrtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  webrtc::LogMessage::GetLogToDebug() << std::endl;

	webrtc::ThreadManager::Instance()->WrapCurrentThread();
	webrtc::Thread* thread = webrtc::Thread::Current();
	webrtc::InitializeSSL();

	// webrtc server
	std::list<std::string> iceServerList;
	if ((strlen(stunurl) != 0) && (strcmp(stunurl,"-") != 0)) {
		iceServerList.push_back(std::string("stun:")+stunurl);
	}
	if (strlen(turnurl)) {
		iceServerList.push_back(std::string("turn:")+turnurl);
	}

	// init trials fields
	webrtc::field_trial::InitFieldTrialsFromString(webrtcTrialsFields.c_str());

	webRtcServer = new PeerConnectionManager(iceServerList, config["urls"], audioLayer, publishFilter, localWebrtcUdpPortRange, useNullCodec, usePlanB, maxpc, transportType, basePath);
	if (!webRtcServer->InitializePeerConnection())
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
		if (!disableXframeOptions) {
			options.push_back("additional_header");
			options.push_back("X-Frame-Options: SAMEORIGIN");
		}
		options.push_back("access_control_allow_origin");
		options.push_back("*");
		options.push_back("listening_ports");
		options.push_back(httpAddress);
		options.push_back("enable_keep_alive");
		options.push_back("yes");
		options.push_back("keep_alive_timeout_ms");
		options.push_back("1000");
		options.push_back("decode_url");
		options.push_back("no");
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
			std::map<std::string,HttpServerRequestHandler::httpFunction> func = webRtcServer->getHttpApi();
			std::cout << "HTTP Listen at " << httpAddress << std::endl;
			HttpServerRequestHandler httpServer(func, options);

			// start STUN server if needed
			std::unique_ptr<webrtc::StunServer> stunserver;
			if (localstunurl != NULL)
			{
				webrtc::SocketAddress server_addr;
				server_addr.FromString(localstunurl);
				webrtc::AsyncUDPSocket* server_socket = webrtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
				if (server_socket)
				{
					stunserver.reset(new webrtc::StunServer(server_socket));
					std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
				}
			}

			// start TRUN server if needed
			std::unique_ptr<webrtc::TurnServer> turnserver;
			if (localturnurl != NULL)
			{
				std::istringstream is(localturnurl);
				std::string addr;
				std::getline(is, addr, '@');
				std::getline(is, addr, '@');
				webrtc::SocketAddress server_addr;
				server_addr.FromString(addr);
				turnserver.reset(new webrtc::TurnServer(webrtc::Thread::Current()));
				turnserver->set_auth_hook(&turnAuth);
				turnserver->set_redirect_hook(&turnRedirector);

				webrtc::Socket* tcp_server_socket = thread->socketserver()->CreateSocket(AF_INET, SOCK_STREAM);
				if (tcp_server_socket) {
					std::cout << "TURN Listening TCP at " << server_addr.ToString() << std::endl;
					tcp_server_socket->Bind(server_addr);
					tcp_server_socket->Listen(5);
					turnserver->AddInternalServerSocket(tcp_server_socket, webrtc::PROTO_TCP);
				} else {
					std::cout << "Failed to create TURN TCP server socket" << std::endl;
				}

				webrtc::AsyncUDPSocket* udp_server_socket = webrtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
				if (udp_server_socket) {
					std::cout << "TURN Listening UDP at " << server_addr.ToString() << std::endl;
					turnserver->AddInternalSocket(udp_server_socket, webrtc::PROTO_UDP);
				} else {
					std::cout << "Failed to create TURN UDP server socket" << std::endl;
				}

				is.clear();
				is.str(turnurl);
				std::getline(is, addr, '@');
				std::getline(is, addr, '@');
				webrtc::SocketAddress external_server_addr;
				external_server_addr.FromString(addr);		
				std::cout << "TURN external addr:" << external_server_addr.ToString() << std::endl;			
				turnserver->SetExternalSocketFactory(new webrtc::BasicPacketSocketFactory(thread->socketserver()), webrtc::SocketAddress(external_server_addr.ipaddr(), 0));
			}
			
			// mainloop
			signal(SIGINT,sighandler);
			thread->Run();

		} catch (const CivetException & ex) {
			std::cout << "Cannot Initialize start HTTP server exception:" << ex.what() << std::endl;
		}
	}

	webrtc::CleanupSSL();
	std::cout << "Exit" << std::endl;
	return 0;
}


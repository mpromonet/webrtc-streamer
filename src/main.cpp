/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>

#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "p2p/base/stun_server.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"

#if WIN32
#include "getopt.h"
#endif

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	const char* turnurl       = "";
	const char* defaultlocalstunurl  = "0.0.0.0:3478";
	const char* localstunurl  = NULL;
	const char* stunurl       = "stun.l.google.com:19302";
	int logLevel              = rtc::LERROR;
	const char* webroot       = "./html";
	std::string sslCertificate;
	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kPlatformDefaultAudio;
	std::string streamName;
	std::map<std::string,std::string> urlVideoList;
	std::map<std::string,std::string> urlAudioList;
	std::string nbthreads;
	std::string passwdFile;
	std::string authDomain = "mydomain.com";
	std::string publishFilter(".*");

	std::string httpAddress("0.0.0.0:");
	std::string httpPort = "8000";
	const char * port = getenv("PORT");
	if (port)
	{
		httpPort = port;
	}
	httpAddress.append(httpPort);

	int c = 0;
	while ((c = getopt (argc, argv, "hVv::" "c:H:w:T:A:D:C:" "t:S::s::" "a::q:" "n:u:U:")) != -1)
	{
		switch (c)
		{
			case 'H': httpAddress = optarg; break;
			case 'c': sslCertificate = optarg; break;
			case 'w': webroot = optarg; break;
			case 'T': nbthreads = optarg; break;
			case 'A': passwdFile = optarg; break;
			case 'D': authDomain = optarg; break;

			case 't': turnurl = optarg; break;
			case 'S': localstunurl = optarg ? optarg : defaultlocalstunurl; stunurl = localstunurl; break;
			case 's': localstunurl = NULL; stunurl = optarg ? optarg : defaultlocalstunurl; break;
			
			case 'a': audioLayer = optarg ? (webrtc::AudioDeviceModule::AudioLayer)atoi(optarg) : webrtc::AudioDeviceModule::kDummyAudio; break;
			case 'q': publishFilter = optarg ; break;
				
			case 'C': {
				Json::Value root;  
				std::ifstream stream(optarg);
				stream >> root;
				if (root.isMember("urls")) {
					Json::Value urls = root["urls"];
					for( auto it = urls.begin() ; it != urls.end() ; it++ ) {
						std::string name = it.key().asString();
						Json::Value value = *it;
						if (value.isMember("video")) {
							urlVideoList[name]=value["video"].asString();
						} 
						if (value.isMember("audio")) {
							urlAudioList[name]=value["audio"].asString();
						} 
					}
				}
				break;
			}

			case 'n': streamName = optarg; break;
			case 'u': {
				if (!streamName.empty()) {
					urlVideoList[streamName]=optarg;
				}
			}
			break;
			case 'U': {
				if (!streamName.empty()) {
					urlAudioList[streamName]=optarg;
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
				std::cout << "\t -T nbthreads       : number of threads for HTTP server"                                          << std::endl;
				std::cout << "\t -A passwd          : password file for HTTP server access"                                          << std::endl;
				std::cout << "\t -D authDomain      : authentication domain for HTTP server access (default:mydomain.com)"                                       << std::endl;
			
				std::cout << "\t -S[stun_address]                   : start embeded STUN server bind to address (default " << defaultlocalstunurl << ")" << std::endl;
				std::cout << "\t -s[stun_address]                   : use an external STUN server (default " << stunurl << ")"                    << std::endl;
				std::cout << "\t -t[username:password@]turn_address : use an external TURN relay server (default disabled)"       << std::endl;

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
		urlVideoList[url]=url;
		optind++;
	}

	rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetLogToDebug() << std::endl;

	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();

	// webrtc server
	std::list<std::string> iceServerList;
	iceServerList.push_back(std::string("stun:")+stunurl);
	if (strlen(turnurl)) {
		iceServerList.push_back(std::string("turn:")+turnurl);
	}
	PeerConnectionManager webRtcServer(iceServerList, urlVideoList, urlAudioList, audioLayer, publishFilter);
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
		options.push_back("access_control_allow_origin");
		options.push_back("*");
		options.push_back("listening_ports");
		options.push_back(httpAddress);
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

		// http api callbacks
		std::map<std::string,HttpServerRequestHandler::httpFunction> func;
		func["/api/getMediaList"]          = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getMediaList();
		};
		
		func["/api/getVideoDeviceList"]    = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getVideoDeviceList();
		};
		
		func["/api/getAudioDeviceList"]    = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getAudioDeviceList();
		};
		
		func["/api/getIceServers"]         = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getIceServers(req_info->remote_addr);
		};
		
		func["/api/call"]                  = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			std::string url;
			std::string audiourl;
			std::string options;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
				CivetServer::getParam(req_info->query_string, "url", url);
				CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
				CivetServer::getParam(req_info->query_string, "options", options);
			}
			return webRtcServer.call(peerid, url, audiourl, options, in);
		};
		
		func["/api/hangup"]                = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
			}
			return webRtcServer.hangUp(peerid);
		};
		
		func["/api/createOffer"]           = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			std::string url;
			std::string audiourl;
			std::string options;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
				CivetServer::getParam(req_info->query_string, "url", url);
				CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
				CivetServer::getParam(req_info->query_string, "options", options);
			}
			return webRtcServer.createOffer(peerid, url, audiourl, options);
		};
		func["/api/setAnswer"]             = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
			}
			webRtcServer.setAnswer(peerid, in);
			Json::Value answer(1);
			return answer;
		};
		
		func["/api/getIceCandidate"]       = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
			}
			return webRtcServer.getIceCandidateList(peerid);
		};
		
		func["/api/addIceCandidate"]       = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			std::string peerid;
			if (req_info->query_string) {
				CivetServer::getParam(req_info->query_string, "peerid", peerid);
			}
			return webRtcServer.addIceCandidate(peerid, in);
		};
		
		func["/api/getPeerConnectionList"] = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getPeerConnectionList();
		};
		
		func["/api/getStreamList"]         = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
			return webRtcServer.getStreamList();
		};

		func["/api/version"]               = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
        		Json::Value answer(VERSION);
		        return answer;
		};
                func["/api/log"]                   = [&webRtcServer](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value {
                        std::string loglevel;
                        if (req_info->query_string) {
                            CivetServer::getParam(req_info->query_string, "level", loglevel);
                            if (!loglevel.empty()) {
                                rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)atoi(loglevel.c_str()));
                            }
                        }
                        Json::Value answer(rtc::LogMessage::GetLogToDebug());
                        return answer;
                };
		func["/api/help"]           = [func](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
			Json::Value answer;
			for (auto it : func) {
			    answer.append(it.first);
			}
			return answer;
		};		
		
		try {
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
			
			// mainloop
			thread->Run();

		} catch (const CivetException & ex) {
			std::cout << "Cannot Initialize start HTTP server exception:" << ex.what() << std::endl;
		}
	}

	rtc::CleanupSSL();
	return 0;
}


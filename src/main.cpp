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

#include "cxxopts.hpp"

#include "api/environment/environment_factory.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "p2p/test/stun_server.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/test/turn_server.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"

PeerConnectionManager *webRtcServer = NULL;

void sighandler(int n)
{
	printf("SIGINT\n");
	// delete need thread still running
	delete webRtcServer;
	webRtcServer = NULL;
	webrtc::Thread::Current()->Quit();
}

class TurnAuth : public webrtc::TurnAuthInterface
{
public:
	virtual bool GetKey(absl::string_view username, absl::string_view realm, std::string *key)
	{
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

std::string GetExeDir(const char *argv0)
{
	std::string exeDir;
#ifdef _WIN32
	char resolvedPath[MAX_PATH];
	if (GetModuleFileNameA(NULL, resolvedPath, MAX_PATH))
	{
		std::string fullPath(resolvedPath);
		size_t pos = fullPath.find_last_of("\\/");
		if (pos != std::string::npos)
		{
			exeDir = fullPath.substr(0, pos + 1);
		}
	}
#else
	char resolvedPath[PATH_MAX];
	if (realpath(argv0, resolvedPath))
	{
		std::string fullPath(resolvedPath);
		exeDir = dirname(resolvedPath);
		exeDir += "/";
	}
#endif
	return exeDir;
}

std::string GetDefaultRessourceDir(const char *argv0)
{

	std::string ressourceDir(WEBRTCSTREAMERRESSOURCE);
	if (ressourceDir[0] != '/')
	{
		std::string exeDir = GetExeDir(argv0);
		ressourceDir = exeDir + ressourceDir;
	}

	return ressourceDir;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	std::string turnurl = "";
	const char *defaultlocalstunurl = "0.0.0.0:3478";
	std::string localstunurl;
	const char *defaultlocalturnurl = "turn:turn@0.0.0.0:3478";
	std::string localturnurl;
	std::string stunurl = "stun.l.google.com:19302";
	std::string localWebrtcUdpPortRange = "0:65535";
	int logLevel = webrtc::LS_NONE;
	std::string webroot = GetDefaultRessourceDir(argv[0]);
	std::string basePath;
	std::string sslCertificate;
	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kPlatformDefaultAudio;
	std::string nbthreads;
	std::string passwdFile;
	std::string authDomain = "mydomain.com";
	bool disableXframeOptions = false;

	std::string publishFilter(".*");
	Json::Value config;
	bool useNullCodec = false;
	bool usePlanB = false;
	int maxpc = 0;
	webrtc::PeerConnectionInterface::IceTransportsType transportType = webrtc::PeerConnectionInterface::IceTransportsType::kAll;
	std::string webrtcTrialsFields = "WebRTC-FrameDropper/Disabled/WebRTC-Video-H26xPacketBuffer/Enabled/";
	TurnAuth turnAuth;
	TurnRedirector turnRedirector;

	std::string httpAddress("0.0.0.0:");
	std::string httpPort = "8000";
	const char *port = getenv("PORT");
	if (port)
	{
		httpPort = port;
	}
	httpAddress.append(httpPort);

	std::string streamName;

	try
	{
		cxxopts::Options options(argv[0], "WebRTC streamer");

		options.add_options("General")
			("h,help", "Print help")
			("V,version", "Print version")
			("v,verbose", "Verbosity level (use multiple times for more verbosity)")
			("C,config", "Load urls from JSON config file", cxxopts::value<std::string>())
			("n,name", "Register a stream with name", cxxopts::value<std::string>())
			("u,video", "Video URL for the named stream", cxxopts::value<std::string>())
			("U,audio", "Audio URL for the named stream", cxxopts::value<std::string>())
			("urls", "URLs to register in the source list", cxxopts::value<std::vector<std::string>>());

		options.add_options("HTTP")
			("H,http", "HTTP server binding (default 0.0.0.0:8000)", cxxopts::value<std::string>())
			("w,webroot", "Path to get static files", cxxopts::value<std::string>())
			("c,cert", "Path to private key and certificate for HTTPS", cxxopts::value<std::string>())
			("N,threads", "Number of threads for HTTP server", cxxopts::value<std::string>())
			("A,passwd", "Password file for HTTP server access", cxxopts::value<std::string>())
			("D,domain", "Authentication domain for HTTP server access (default:mydomain.com)", cxxopts::value<std::string>())
			("X,disable-xframe", "Disable X-Frame-Options header")
			("B,base-path", "Base path for HTTP server", cxxopts::value<std::string>());

		options.add_options("WebRTC")
			("m,maxpc", "Maximum number of peer connections", cxxopts::value<int>())
			("I,ice-transport", "Set ice transport type", cxxopts::value<int>())
			("T,turn-server", "Start embedded TURN server", cxxopts::value<std::string>()->implicit_value(defaultlocalturnurl))
			("t,turn", "Use an external TURN relay server", cxxopts::value<std::string>())
			("S,stun-server", "Start embedded STUN server bind to address", cxxopts::value<std::string>()->implicit_value(defaultlocalstunurl))
			("s,stun", "Use an external STUN server", cxxopts::value<std::string>()->implicit_value(defaultlocalstunurl))
			("R,udp-range", "Set the webrtc udp port range", cxxopts::value<std::string>())
			("W,trials", "Set the webrtc trials fields", cxxopts::value<std::string>())
			("a,audio-layer", "Specify audio capture layer to use (omit value for dummy audio)", cxxopts::value<std::string>()->implicit_value(""))
			("q,publish-filter", "Specify publish filter", cxxopts::value<std::string>())
			("o,null-codec", "Use null codec (keep frame encoded)")
			("b,plan-b", "Use sdp plan-B (default use unifiedPlan)");

		options.parse_positional({"urls"});
		options.positional_help("[urls...]");

		auto result = options.parse(argc, argv);

		if (result.count("help"))
		{
			std::cout << options.help() << std::endl;
			exit(0);
		}

		if (result.count("version"))
		{
			std::cout << VERSION << std::endl;
			exit(0);
		}

		if (result.count("verbose"))
		{
			logLevel -= result.count("verbose");
		}

		if (result.count("config"))
		{
			std::string configFile = result["config"].as<std::string>();
			std::ifstream stream(configFile);
			stream >> config;
		}

		if (result.count("name"))
		{
			streamName = result["name"].as<std::string>();
		}

		if (result.count("video") && !streamName.empty())
		{
			config["urls"][streamName]["video"] = result["video"].as<std::string>();
		}

		if (result.count("audio") && !streamName.empty())
		{
			config["urls"][streamName]["audio"] = result["audio"].as<std::string>();
		}

		if (result.count("http"))
		{
			httpAddress = result["http"].as<std::string>();
		}

		if (result.count("webroot"))
		{
			webroot = result["webroot"].as<std::string>();
		}

		if (result.count("cert"))
		{
			sslCertificate = result["cert"].as<std::string>();
		}

		if (result.count("threads"))
		{
			nbthreads = result["threads"].as<std::string>();
		}

		if (result.count("passwd"))
		{
			passwdFile = result["passwd"].as<std::string>();
		}

		if (result.count("domain"))
		{
			authDomain = result["domain"].as<std::string>();
		}

		if (result.count("disable-xframe"))
		{
			disableXframeOptions = true;
		}

		if (result.count("base-path"))
		{
			basePath = result["base-path"].as<std::string>();
		}

		if (result.count("maxpc"))
		{
			maxpc = result["maxpc"].as<int>();
		}

		if (result.count("ice-transport"))
		{
			transportType = (webrtc::PeerConnectionInterface::IceTransportsType)result["ice-transport"].as<int>();
		}

		if (result.count("turn-server"))
		{
			localturnurl = result["turn-server"].as<std::string>();
			turnurl = localturnurl;
		}

		if (result.count("turn"))
		{
			turnurl = result["turn"].as<std::string>();
		}

		if (result.count("stun-server"))
		{
			localstunurl = result["stun-server"].as<std::string>();
			stunurl = localstunurl;
		}

		if (result.count("stun"))
		{
			stunurl = result["stun"].as<std::string>();
		}

		if (result.count("udp-range"))
		{
			localWebrtcUdpPortRange = result["udp-range"].as<std::string>();
		}

		if (result.count("trials"))
		{
			webrtcTrialsFields = result["trials"].as<std::string>();
		}

		if (result.count("audio-layer"))
		{
			std::string audioValue = result["audio-layer"].as<std::string>();
			if (audioValue.empty())
			{
				audioLayer = webrtc::AudioDeviceModule::kDummyAudio;
			}
			else
			{
				audioLayer = (webrtc::AudioDeviceModule::AudioLayer)atoi(audioValue.c_str());
			}
		}

		if (result.count("publish-filter"))
		{
			publishFilter = result["publish-filter"].as<std::string>();
		}

		if (result.count("null-codec"))
		{
			useNullCodec = true;
		}

		if (result.count("plan-b"))
		{
			usePlanB = true;
		}

		if (result.count("urls"))
		{
			auto urls = result["urls"].as<std::vector<std::string>>();
			for (const auto& url : urls)
			{
				config["urls"][url]["video"] = url;
			}
		}
	}
	catch (const cxxopts::exceptions::exception& e)
	{
		std::cerr << "Error parsing options: " << e.what() << std::endl;
		exit(1);
	}

	std::cout << "Version:" << VERSION << std::endl;

	std::cout << config;

	webrtc::LogMessage::LogToDebug((webrtc::LoggingSeverity)logLevel);
	webrtc::LogMessage::LogTimestamps();
	webrtc::LogMessage::LogThreads();
	std::cout << "Logger level:" << webrtc::LogMessage::GetLogToDebug() << std::endl;

	webrtc::ThreadManager::Instance()->WrapCurrentThread();
	webrtc::Thread *thread = webrtc::Thread::Current();
	webrtc::InitializeSSL();

	// webrtc server
	std::list<std::string> iceServerList;
	if (!stunurl.empty() && stunurl != "-")
	{
		iceServerList.push_back(std::string("stun:") + stunurl);
	}
	if (!turnurl.empty())
	{
		iceServerList.push_back(std::string("turn:") + turnurl);
	}

	webRtcServer = new PeerConnectionManager(iceServerList, config["urls"], audioLayer, publishFilter, localWebrtcUdpPortRange, useNullCodec, usePlanB, maxpc, transportType, basePath, webrtcTrialsFields);
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
		if (!disableXframeOptions)
		{
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
		if (!sslCertificate.empty())
		{
			options.push_back("ssl_certificate");
			options.push_back(sslCertificate);
		}
		if (!nbthreads.empty())
		{
			options.push_back("num_threads");
			options.push_back(nbthreads);
		}
		if (!passwdFile.empty())
		{
			options.push_back("global_auth_file");
			options.push_back(passwdFile);
			options.push_back("authentication_domain");
			options.push_back(authDomain);
		}

		try
		{
			std::map<std::string, HttpServerRequestHandler::httpFunction> func = webRtcServer->getHttpApi();
			std::cout << "HTTP Listen at " << httpAddress << std::endl;
			HttpServerRequestHandler httpServer(func, options);

			webrtc::Environment env(webrtc::CreateEnvironment());
			// start STUN server if needed
			std::unique_ptr<webrtc::StunServer> stunserver;
			if (!localstunurl.empty())
			{
				webrtc::SocketAddress server_addr;
				server_addr.FromString(localstunurl);
				std::unique_ptr<webrtc::AsyncUDPSocket> server_socket = webrtc::AsyncUDPSocket::Create(env, server_addr, *thread->socketserver());
				stunserver.reset(new webrtc::StunServer(std::move(server_socket)));
				std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
			}

			// start TRUN server if needed
			std::unique_ptr<webrtc::TurnServer> turnserver;
			if (!localturnurl.empty())
			{
				std::istringstream is(localturnurl);
				std::string addr;
				std::getline(is, addr, '@');
				std::getline(is, addr, '@');
				webrtc::SocketAddress server_addr;
				server_addr.FromString(addr);
				turnserver.reset(new webrtc::TurnServer(env, webrtc::Thread::Current()));
				turnserver->set_auth_hook(&turnAuth);
				turnserver->set_redirect_hook(&turnRedirector);

				std::unique_ptr<webrtc::Socket> tcp_server_socket(thread->socketserver()->CreateSocket(AF_INET, SOCK_STREAM));
				if (tcp_server_socket)
				{
					std::cout << "TURN Listening TCP at " << server_addr.ToString() << std::endl;
					tcp_server_socket->Bind(server_addr);
					tcp_server_socket->Listen(5);
					turnserver->AddInternalServerSocket(std::move(tcp_server_socket), webrtc::PROTO_TCP);
				}
				else
				{
					std::cout << "Failed to create TURN TCP server socket" << std::endl;
				}

				std::unique_ptr<webrtc::AsyncUDPSocket> udp_server_socket = webrtc::AsyncUDPSocket::Create(env, server_addr, *thread->socketserver());
				if (udp_server_socket)
				{
					std::cout << "TURN Listening UDP at " << server_addr.ToString() << std::endl;
					turnserver->AddInternalSocket(std::move(udp_server_socket), webrtc::PROTO_UDP);
				}
				else
				{
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
			signal(SIGINT, sighandler);
			thread->Run();
		}
		catch (const CivetException &ex)
		{
			std::cout << "Cannot Initialize start HTTP server exception:" << ex.what() << std::endl;
		}
	}

	webrtc::CleanupSSL();
	std::cout << "Exit" << std::endl;
	return 0;
}

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <utility>
#include <functional>

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "api/enable_media.h"
#include "api/audio/create_audio_device_module.h"

#include "PeerConnectionManager.h"
#include "V4l2AlsaMap.h"
#include "CapturerFactory.h"

#include "VideoEncoderFactory.h"
#include "VideoDecoderFactory.h"


// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

// character to remove from url to make webrtc label
bool ignoreInLabel(char c)
{
	return c == ' ' || c == ':' || c == '.' || c == '/' || c == '&';
}

/* ---------------------------------------------------------------------------
**  helpers that should be moved somewhere else
** -------------------------------------------------------------------------*/

#ifdef WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <net/if.h>
#include <ifaddrs.h>
#endif
std::string getServerIpFromClientIp(long clientip)
{
	std::string serverAddress("127.0.0.1");
#ifdef WIN32
    ULONG outBufLen = 0;
    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
        if (pAddresses != NULL) {
            if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR) {

                for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {

                    for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                        sockaddr* sa = pUnicast->Address.lpSockaddr;

                        if (sa->sa_family == AF_INET) {
                            struct sockaddr_in* ipv4 = (struct sockaddr_in*)sa;
                            struct in_addr addr = ipv4->sin_addr;

                            struct in_addr mask;
                            mask.s_addr = htonl((0xFFFFFFFFU << (32 - pUnicast->OnLinkPrefixLength)) & 0xFFFFFFFFU);

                            if ((addr.s_addr & mask.s_addr) == (clientip & mask.s_addr)) {
								serverAddress = inet_ntoa(addr);
                                break;
                            }
                        }
                    }
                }
            }
        }
        free(pAddresses);
    }
#else
	struct ifaddrs *ifaddr = NULL;
	if (getifaddrs(&ifaddr) == 0)
	{
		for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
		{
			if ((ifa->ifa_netmask != NULL) && (ifa->ifa_netmask->sa_family == AF_INET) && (ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family == AF_INET))
			{
				struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
				struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
				if ((addr->sin_addr.s_addr & mask->sin_addr.s_addr) == (clientip & mask->sin_addr.s_addr))
				{
					char host[NI_MAXHOST];
					if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0)
					{
						serverAddress = host;
						break;
					}
				}
			}
		}
	}
	freeifaddrs(ifaddr);
#endif
	return serverAddress;
}

webrtc::PeerConnectionInterface::IceServer getIceServerFromUrl(const std::string &url, const std::string &clientIp = "")
{
	webrtc::PeerConnectionInterface::IceServer srv;
	srv.uri = url;

	std::size_t pos = url.find_first_of(':');
	if (pos != std::string::npos)
	{
		std::string protocol = url.substr(0, pos);
		std::string uri = url.substr(pos + 1);
		std::string credentials;

		std::size_t pos = uri.rfind('@');
		if (pos != std::string::npos)
		{
			credentials = uri.substr(0, pos);
			uri = uri.substr(pos + 1);
		}

		if ((uri.find("0.0.0.0:") == 0) && (clientIp.empty() == false))
		{
			// answer with ip that is on same network as client
			std::string clienturl = getServerIpFromClientIp(inet_addr(clientIp.c_str()));
			clienturl += uri.substr(uri.find_first_of(':'));
			uri = clienturl;
		}
		srv.uri = protocol + ":" + uri;

		if (!credentials.empty())
		{
			pos = credentials.find(':');
			if (pos == std::string::npos)
			{
				srv.username = credentials;
			}
			else
			{
				srv.username = credentials.substr(0, pos);
				srv.password = credentials.substr(pos + 1);
			}
		}
	}

	return srv;
}

std::unique_ptr<webrtc::VideoEncoderFactory> CreateEncoderFactory(bool nullCodec) {
	std::unique_ptr<webrtc::VideoEncoderFactory> factory;
	if (nullCodec) {
		factory = std::make_unique<VideoEncoderFactory>();
	} else {
		factory = webrtc::CreateBuiltinVideoEncoderFactory();
	}
	return factory;
}

std::unique_ptr<webrtc::VideoDecoderFactory> CreateDecoderFactory(bool nullCodec) {
	std::unique_ptr<webrtc::VideoDecoderFactory> factory;
	if (nullCodec) {
		factory = std::make_unique<VideoDecoderFactory>();
	} else {
		factory = webrtc::CreateBuiltinVideoDecoderFactory();
	}
	return factory;
}

webrtc::PeerConnectionFactoryDependencies CreatePeerConnectionFactoryDependencies(webrtc::Thread* signalingThread, webrtc::Thread* workerThread, webrtc::scoped_refptr<webrtc::AudioDeviceModule> audioDeviceModule, webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderfactory, bool useNullCodec)
{
	webrtc::PeerConnectionFactoryDependencies dependencies;
	dependencies.network_thread = NULL;
	dependencies.worker_thread = workerThread;
	dependencies.signaling_thread = signalingThread;
	dependencies.event_log_factory = absl::make_unique<webrtc::RtcEventLogFactory>();

	dependencies.adm = std::move(audioDeviceModule);
	dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
	dependencies.audio_decoder_factory = std::move(audioDecoderfactory);

	dependencies.video_encoder_factory = CreateEncoderFactory(useNullCodec);
	dependencies.video_decoder_factory = CreateDecoderFactory(useNullCodec);

	webrtc::EnableMedia(dependencies);

	return dependencies;
}


std::string getParam(const char *queryString, const char *paramName) {
	std::string value;
	if (queryString)
	{
		CivetServer::getParam(queryString, paramName, value);
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionManager(const std::list<std::string> &iceServerList, const Json::Value & config, const webrtc::AudioDeviceModule::AudioLayer audioLayer, const std::string &publishFilter, const std::string & webrtcUdpPortRange, bool useNullCodec, bool usePlanB, int maxpc, webrtc::PeerConnectionInterface::IceTransportsType transportType, const std::string & basePath)
	: m_webrtcenv(webrtc::CreateEnvironment()),
	  m_signalingThread(webrtc::Thread::Create()),
	  m_workerThread(webrtc::Thread::Create()),
	  m_audioDecoderfactory(webrtc::CreateBuiltinAudioDecoderFactory()), 
  	  m_video_decoder_factory(CreateDecoderFactory(useNullCodec)),
	  m_iceServerList(iceServerList), 
	  m_config(config),
	  m_publishFilter(publishFilter), 
	  m_webrtcPortRange(webrtcUdpPortRange),
	  m_useNullCodec(useNullCodec), 
	  m_usePlanB(usePlanB),
	  m_maxpc(maxpc),
	  m_transportType(transportType)
{
	m_workerThread->SetName("worker", NULL);
	m_workerThread->Start();

	m_workerThread->BlockingCall([this, audioLayer] {
		this->createAudioModule(audioLayer);
    });
		
	m_signalingThread->SetName("signaling", NULL);
	m_signalingThread->Start();
	m_peer_connection_factory = webrtc::CreateModularPeerConnectionFactory(CreatePeerConnectionFactoryDependencies(m_signalingThread.get(), m_workerThread.get(), m_audioDeviceModule, m_audioDecoderfactory, useNullCodec));

	// build video audio map
	m_videoaudiomap = getV4l2AlsaMap();

	// register api in http server
	m_func[basePath + "/api/getMediaList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getMediaList());
	};

	m_func[basePath + "/api/getVideoDeviceList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getVideoDeviceList());
	};

	m_func[basePath + "/api/getAudioDeviceList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getAudioDeviceList());
	};

	m_func[basePath + "/api/getAudioPlayoutList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getAudioPlayoutList());
	};

	m_func[basePath + "/api/getIceServers"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getIceServers(req_info->remote_addr));
	};

	m_func[basePath + "/api/call"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {	
		std::string peerid   = getParam(req_info->query_string, "peerid");
		std::string url      = getParam(req_info->query_string, "url");
		std::string audiourl = getParam(req_info->query_string, "audiourl");
		std::string options  = getParam(req_info->query_string, "options");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->call(peerid, url, audiourl, options, in));
	};

	m_func[basePath + "/api/whep"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		std::string videourl      = getParam(req_info->query_string, "url");
		std::string audiourl = getParam(req_info->query_string, "audiourl");
		std::string options  = getParam(req_info->query_string, "options");
		std::string url(req_info->request_uri);
		url.append("?").append(req_info->query_string);		
		return this->whep(req_info->request_method, url, peerid, videourl, audiourl, options, in);	
	};

	m_func[basePath + "/api/hangup"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->hangUp(peerid));
	};

	m_func[basePath + "/api/createOffer"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		std::string url      = getParam(req_info->query_string, "url");
		std::string audiourl = getParam(req_info->query_string, "audiourl");
		std::string options  = getParam(req_info->query_string, "options");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->createOffer(peerid, url, audiourl, options));
	};
	m_func[basePath + "/api/setAnswer"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->setAnswer(peerid, in));
	};

	m_func[basePath + "/api/getIceCandidate"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getIceCandidateList(peerid));
	};

	m_func[basePath + "/api/addIceCandidate"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string peerid   = getParam(req_info->query_string, "peerid");
		return std::make_tuple(200, std::map<std::string,std::string>(),this->addIceCandidate(peerid, in));
	};

	m_func[basePath + "/api/getPeerConnectionList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getPeerConnectionList());
	};

	m_func[basePath + "/api/getStreamList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		return std::make_tuple(200, std::map<std::string,std::string>(),this->getStreamList());
	};

	m_func[basePath + "/api/version"] = [](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		Json::Value answer(VERSION);
		return std::make_tuple(200, std::map<std::string,std::string>(), answer);
	};
	m_func[basePath + "/api/log"] = [](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		std::string loglevel   = getParam(req_info->query_string, "level");
		if (!loglevel.empty())
		{
			webrtc::LogMessage::LogToDebug((webrtc::LoggingSeverity)atoi(loglevel.c_str()));
		}
		Json::Value answer(webrtc::LogMessage::GetLogToDebug());
		return std::make_tuple(200, std::map<std::string,std::string>(), answer);
	};
	m_func[basePath + "/api/help"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> HttpServerRequestHandler::httpFunctionReturn {
		Json::Value answer(Json::ValueType::arrayValue);
		for (auto it : m_func) {
			answer.append(it.first);
		}
		return std::make_tuple(200, std::map<std::string,std::string>(), answer);
	};
}

/* ---------------------------------------------------------------------------
**  Destructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::~PeerConnectionManager() {
	m_workerThread->BlockingCall([this] {
		m_audioDeviceModule->Release();
    });	
}

// from https://stackoverflow.com/a/12468109/3102264
std::string random_string( size_t length )
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}

std::tuple<int, std::map<std::string,std::string>,Json::Value> PeerConnectionManager::whep(const std::string & method,
		const std::string & url,
		const std::string & requestPeerId, 
		const std::string & videourl,
		const std::string & audiourl,
		const std::string & options,
		const Json::Value &in) {

	int httpcode = 501;

	std::string locationurl(url);
	std::string peerid(requestPeerId);
	if (peerid.empty()) {
		peerid = random_string(32);
		locationurl.append("&").append("peerid=").append(peerid);
	}
	std::map<std::string,std::string> headers;
	std::string answersdp;
	if (method == "DELETE") {
		this->hangUp(peerid);
	} else if (method == "PATCH") {
		RTC_LOG(LS_INFO) << "PATCH\n" << in.asString();
		std::istringstream is(in.asString());
		std::string str;
		std::string mid;
    	while(std::getline(is,str)) {
			str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
			if (strstr(str.c_str(),"a=mid:")) {
				mid = str.substr(strlen("a=mid:"));
			} else if (strstr(str.c_str(),"a=candidate")) {
				std::string sdp = str.substr(strlen("a="));
				std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(mid, 0, sdp, NULL));
				if (!candidate.get()) {
					RTC_LOG(LS_WARNING) << "Can't parse received candidate message.";
				} else {
					std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
					webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
					if (peerConnection) {
						if (!peerConnection->AddIceCandidate(candidate.get())) {
							RTC_LOG(LS_WARNING) << "Failed to apply the received candidate";
						} else {
							httpcode = 200;
						}
					}
				}
			} else if (strstr(str.c_str(),"a=end-of-candidates")) {
				RTC_LOG(LS_INFO) << "end of candidate";
				httpcode = 200;
			}
    	}

	} else {
		std::string offersdp(in.asString());
		RTC_LOG(LS_WARNING) << "offer:" << offersdp;
		std::unique_ptr<webrtc::SessionDescriptionInterface> desc = this->getAnswer(peerid, offersdp, videourl, audiourl, options, true);
		if (desc.get()) {
			desc->ToString(&answersdp);
			headers["Location"] = locationurl;
			headers["Access-Control-Expose-Headers"] = "Location";
			headers["Content-Type"] = "application/sdp";

			httpcode = 201;
		} else {
			RTC_LOG(LS_ERROR) << "Failed to create answer - no SDP";
		}
		RTC_LOG(LS_WARNING) << "anwser:" << answersdp;

	}
	return std::make_tuple(httpcode, headers, answersdp);
}

void PeerConnectionManager::createAudioModule(webrtc::AudioDeviceModule::AudioLayer audioLayer) {
#ifdef HAVE_SOUND
	m_audioDeviceModule = webrtc::CreateAudioDeviceModule(m_webrtcenv, audioLayer);
	if (m_audioDeviceModule->Init() != 0) {
		RTC_LOG(LS_WARNING) << "audio init fails -> disable audio capture";
		m_audioDeviceModule = new webrtc::FakeAudioDeviceModule();
	}
#else
	m_audioDeviceModule = new webrtc::FakeAudioDeviceModule();
#endif	
}
/* ---------------------------------------------------------------------------
**  return deviceList as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getMediaList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> videoCaptureDevice = CapturerFactory::GetVideoCaptureDeviceList(m_publishFilter, m_useNullCodec);
	for (auto videoDevice : videoCaptureDevice)
	{
		Json::Value media;
		media["video"] = videoDevice;

		std::map<std::string, std::string>::iterator it = m_videoaudiomap.find(videoDevice);
		if (it != m_videoaudiomap.end())
		{
			media["audio"] = it->second;
		}
		value.append(media);
	}

	const std::list<std::string> videoList = CapturerFactory::GetVideoSourceList(m_publishFilter, m_useNullCodec);
	for (auto videoSource : videoList)
	{
		Json::Value media;
		media["video"] = videoSource;
		value.append(media);
	}

	for( auto it = m_config.begin() ; it != m_config.end() ; it++ ) {
		std::string name = it.key().asString();
		Json::Value media(*it);
		if (media.isMember("video")) {
			media["video"]=name;
		} 
		if (media.isMember("audio")) {
			media["audio"]=name;
		} 
		value.append(media);
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return video device List as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getVideoDeviceList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> videoCaptureDevice = CapturerFactory::GetVideoCaptureDeviceList(m_publishFilter, m_useNullCodec);
	for (auto videoDevice : videoCaptureDevice)
	{
		value.append(videoDevice);
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return audio device List as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getAudioDeviceList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> audioCaptureDevice = CapturerFactory::GetAudioCaptureDeviceList(m_publishFilter, m_audioDeviceModule);
	for (auto audioDevice : audioCaptureDevice)
	{
		value.append(audioDevice);
	}

	return value;
}

const Json::Value PeerConnectionManager::getAudioPlayoutList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> audioPlayoutDevice = CapturerFactory::GetAudioPlayoutDeviceList(m_publishFilter, m_audioDeviceModule);
	for (auto audioDevice : audioPlayoutDevice)
	{
		value.append(audioDevice);
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return iceServers as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceServers(const std::string &clientIp)
{
	Json::Value urls(Json::arrayValue);

	for (auto iceServer : m_iceServerList)
	{
		Json::Value server;
		Json::Value urlList(Json::arrayValue);
		webrtc::PeerConnectionInterface::IceServer srv = getIceServerFromUrl(iceServer, clientIp);
		RTC_LOG(LS_INFO) << "ICE URL:" << srv.uri;
		if (srv.uri.find("turn:") == 0) {
			urlList.append(srv.uri+"?transport=udp");
			urlList.append(srv.uri+"?transport=tcp");
		} else {
			urlList.append(srv.uri);
		}
		server["urls"] = urlList;
		if (srv.username.length() > 0)
			server["username"] = srv.username;
		if (srv.password.length() > 0)
			server["credential"] = srv.password;
		urls.append(server);
	}

	Json::Value iceServers;
	iceServers["iceServers"] = urls;
	if (m_transportType == webrtc::PeerConnectionInterface::IceTransportsType::kRelay) {
		iceServers["iceTransportPolicy"] = "relay";
	} else {
		iceServers["iceTransportPolicy"] = "all";
	}

	return iceServers;
}

/* ---------------------------------------------------------------------------
**  get PeerConnection associated with peerid
** -------------------------------------------------------------------------*/
webrtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnectionManager::getPeerConnection(const std::string &peerid)
{
	webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
	std::map<std::string, PeerConnectionObserver *>::iterator it = m_peer_connectionobs_map.find(peerid);
	if (it != m_peer_connectionobs_map.end())
	{
		peerConnection = it->second->getPeerConnection();
	}
	return peerConnection;
}
/* ---------------------------------------------------------------------------
**  add ICE candidate to a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::addIceCandidate(const std::string &peerid, const Json::Value &jmessage)
{
	bool result = false;
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (!webrtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) || !webrtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) || !webrtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
	{
		RTC_LOG(LS_WARNING) << "Can't parse received message:" << jmessage;
	}
	else
	{
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));
		if (!candidate.get())
		{
			RTC_LOG(LS_WARNING) << "Can't parse received candidate message.";
		}
		else
		{
			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
			if (peerConnection)
			{
				if (!peerConnection->AddIceCandidate(candidate.get()))
				{
					RTC_LOG(LS_WARNING) << "Failed to apply the received candidate";
				}
				else
				{
					result = true;
				}
			}
		}
	}
	Json::Value answer;
	if (result)
	{
		answer = result;
	}
	return answer;
}

/* ---------------------------------------------------------------------------
** create an offer for a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::createOffer(const std::string &peerid, const std::string &videourl, const std::string &audiourl, const std::string &options)
{
	RTC_LOG(LS_INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options;
	Json::Value offer;

	PeerConnectionObserver *peerConnectionObserver = this->CreatePeerConnection(peerid);
	if (!peerConnectionObserver)
	{
		RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnection";
	}
	else
	{
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();

		if (!this->AddStreams(peerConnection.get(), videourl, audiourl, options))
		{
			RTC_LOG(LS_ERROR) << "Can't add stream";
		} else {
			// register peerid
			{
				std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
				m_peer_connectionobs_map.insert(std::pair<std::string, PeerConnectionObserver *>(peerid, peerConnectionObserver));
			}

			// ask to create offer
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
			rtcoptions.offer_to_receive_video = 0;
			rtcoptions.offer_to_receive_audio = 0;
			std::promise<const webrtc::SessionDescriptionInterface *> localpromise;
			webrtc::scoped_refptr<CreateSessionDescriptionObserver> localSessionObserver(CreateSessionDescriptionObserver::Create(peerConnection, localpromise));
			peerConnection->CreateOffer(localSessionObserver.get(), rtcoptions);

			// waiting for offer
			std::future<const webrtc::SessionDescriptionInterface *> future = localpromise.get_future();
			if (future.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
			{
				// answer with the created offer
				const webrtc::SessionDescriptionInterface *desc = future.get();
				if (desc)
				{
					std::string sdp;
					desc->ToString(&sdp);

					offer[kSessionDescriptionTypeName] = desc->type();
					offer[kSessionDescriptionSdpName] = sdp;
				}
				else
				{
					RTC_LOG(LS_ERROR) << "Failed to create offer - no session";
				}
			}
			else
			{
				localSessionObserver->cancel();
				RTC_LOG(LS_ERROR) << "Failed to create offer - timeout";
			}
		}
	}
	return offer;
}

/* ---------------------------------------------------------------------------
** set answer to a call initiated by createOffer
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::setAnswer(const std::string &peerid, const Json::Value &jmessage)
{
	RTC_LOG(LS_INFO) << jmessage;
	Json::Value answer;

	std::string sdp;
	if (!webrtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(LS_WARNING) << "Can't parse received message.";
		answer["error"] = "Can't parse received message.";
	}
	else
	{
		std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, NULL));
		if (!session_description)
		{
			RTC_LOG(LS_WARNING) << "Can't parse received session description message.";
			answer["error"] = "Can't parse received session description message.";
		}
		else
		{
			RTC_LOG(LS_ERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();

			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
			if (peerConnection)
			{
				std::promise<const webrtc::SessionDescriptionInterface *> remotepromise;
				webrtc::scoped_refptr<SetSessionDescriptionObserver> remoteSessionObserver(SetSessionDescriptionObserver::Create(peerConnection, remotepromise));
				peerConnection->SetRemoteDescription(remoteSessionObserver.get(), session_description.release());
				// waiting for remote description
				std::future<const webrtc::SessionDescriptionInterface *> remotefuture = remotepromise.get_future();
				if (remotefuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
				{
					RTC_LOG(LS_INFO) << "remote_description is ready";
					const webrtc::SessionDescriptionInterface *desc = remotefuture.get();
					if (desc)
					{
						std::string sdp;
						desc->ToString(&sdp);

						answer[kSessionDescriptionTypeName] = desc->type();
						answer[kSessionDescriptionSdpName] = sdp;
					} else {
						answer["error"] = "Can't get remote description.";
					}
				}
				else
				{
					remoteSessionObserver->cancel();
					RTC_LOG(LS_WARNING) << "Can't get remote description.";
					answer["error"] = "Can't get remote description.";
				}
			}
		}
	}
	return answer;
}


std::unique_ptr<webrtc::SessionDescriptionInterface> PeerConnectionManager::getAnswer(const std::string & peerid, const std::string& sdpoffer, const std::string & videourl, const std::string & audiourl, const std::string & options, bool waitgatheringcompletion) {
	std::unique_ptr<webrtc::SessionDescriptionInterface> answer;
	std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdpoffer, NULL));
	if (!session_description) {
		RTC_LOG(LS_WARNING) << "Can't parse received session description message.";
	} else {
		answer = this->getAnswer(peerid, session_description.release(), videourl, audiourl, options, waitgatheringcompletion);
	}
	return answer;
}


std::unique_ptr<webrtc::SessionDescriptionInterface> PeerConnectionManager::getAnswer(const std::string & peerid, webrtc::SessionDescriptionInterface *session_description, const std::string & videourl, const std::string & audiourl, const std::string & options, bool waitgatheringcompletion) {
	std::unique_ptr<webrtc::SessionDescriptionInterface> answer;

	PeerConnectionObserver *peerConnectionObserver = this->CreatePeerConnection(peerid);
	if (!peerConnectionObserver)
	{
		RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnectionObserver";
	}
	else if (!peerConnectionObserver->getPeerConnection().get())
	{
		RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnection";
		delete peerConnectionObserver;
	}
	else
	{
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();
		RTC_LOG(LS_INFO) << "nbStreams local:" << peerConnection->GetSenders().size() << " remote:" << peerConnection->GetReceivers().size() << " localDescription:" << peerConnection->local_description();

		// register peerid
		{
			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			m_peer_connectionobs_map.insert(std::pair<std::string, PeerConnectionObserver *>(peerid, peerConnectionObserver));
		}
		
		// add local stream
		if (!this->AddStreams(peerConnection.get(), videourl, audiourl, options))
		{
			RTC_LOG(LS_ERROR) << "Can't add stream";
		} else {
			// set remote offer
			std::promise<const webrtc::SessionDescriptionInterface *> remotepromise;
			webrtc::scoped_refptr<SetSessionDescriptionObserver> remoteSessionObserver(SetSessionDescriptionObserver::Create(peerConnection, remotepromise));
			peerConnection->SetRemoteDescription(remoteSessionObserver.get(), session_description);
			// waiting for remote description
			std::future<const webrtc::SessionDescriptionInterface *> remotefuture = remotepromise.get_future();
			if (remotefuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
			{
				RTC_LOG(LS_INFO) << "remote_description is ready";
			}
			else
			{
				remoteSessionObserver->cancel();
				RTC_LOG(LS_ERROR) << "remote_description timeout";
			}

			// create answer
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
			std::promise<const webrtc::SessionDescriptionInterface *> localpromise;
			webrtc::scoped_refptr<CreateSessionDescriptionObserver> localSessionObserver(CreateSessionDescriptionObserver::Create(peerConnection, localpromise));
			peerConnection->CreateAnswer(localSessionObserver.get(), rtcoptions);

			// wait gathering completion
			if (waitgatheringcompletion) {
				int retry = 5;
				while ( (peerConnectionObserver->getGatheringState() != webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete) && (retry > 0) ) {
					RTC_LOG(LS_ERROR) << "waiting..." << retry;
					retry --;
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
				}
			}

			// waiting for answer
			std::future<const webrtc::SessionDescriptionInterface *> localfuture = localpromise.get_future();
			if (localfuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
			{
				const webrtc::SessionDescriptionInterface *desc = localfuture.get();
				if (desc)
				{
					answer = desc->Clone();
				}
				else
				{
					RTC_LOG(LS_ERROR) << "Failed to create answer - no SDP";
				}
			}
			else
			{
				RTC_LOG(LS_ERROR) << "Failed to create answer - timeout";
				localSessionObserver->cancel();
			}

		}

	}
	return answer;
}

/* ---------------------------------------------------------------------------
**  auto-answer to a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::call(const std::string &peerid, const std::string &videourl, const std::string &audiourl, const std::string &options, const Json::Value &jmessage)
{
	RTC_LOG(LS_INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options;

	Json::Value answer;

	std::string sdp;

	if (!webrtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(LS_WARNING) << "Can't parse received message.";
	}
	else
	{
		std::unique_ptr<webrtc::SessionDescriptionInterface> desc = this->getAnswer(peerid, sdp, videourl, audiourl, options);
		if (desc.get())
		{
			std::string sdp;
			desc->ToString(&sdp);

			answer[kSessionDescriptionTypeName] = desc->type();
			answer[kSessionDescriptionSdpName] = sdp;
		}
		else
		{
			RTC_LOG(LS_ERROR) << "Failed to create answer - no SDP";
		}
	}
	return answer;
}

bool PeerConnectionManager::streamStillUsed(const std::string &streamLabel)
{
	bool stillUsed = false;
	for (auto it : m_peer_connectionobs_map)
	{
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> localstreams = peerConnection->GetSenders();
		for (auto stream : localstreams)
		{
			std::vector<std::string> streamVector = stream->stream_ids();
			if (streamVector.size() > 0) {	
				if (streamVector[0] == streamLabel)
				{
					stillUsed = true;
					break;
				}						
			}
		}
	}
	return stillUsed;
}

/* ---------------------------------------------------------------------------
**  hangup a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::hangUp(const std::string &peerid)
{
	bool result = false;
	RTC_LOG(LS_INFO) << __FUNCTION__ << " " << peerid;

	PeerConnectionObserver *pcObserver = NULL;
	{
		std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
		std::map<std::string, PeerConnectionObserver *>::iterator it = m_peer_connectionobs_map.find(peerid);
		if (it != m_peer_connectionobs_map.end())
		{
			pcObserver = it->second;
			RTC_LOG(LS_ERROR) << "Remove PeerConnection peerid:" << peerid;
			m_peer_connectionobs_map.erase(it);
		}
	}
	if (pcObserver)
	{
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();

		std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> localstreams = peerConnection->GetSenders();
		for (auto stream : localstreams)
		{
			std::vector<std::string> streamVector = stream->stream_ids();
			if (streamVector.size() > 0) {
				std::string streamLabel = streamVector[0];
				bool stillUsed = this->streamStillUsed(streamLabel);
				if (!stillUsed)
				{
					RTC_LOG(LS_ERROR) << "hangUp stream is no more used " << streamLabel;
					std::lock_guard<std::mutex> mlock(m_streamMapMutex);
					std::map<std::string, std::pair<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, webrtc::scoped_refptr<webrtc::AudioSourceInterface>>>::iterator it = m_stream_map.find(streamLabel);
					if (it != m_stream_map.end())
					{
						m_stream_map.erase(it);
					}

					RTC_LOG(LS_ERROR) << "hangUp stream closed " << streamLabel;
				}

				peerConnection->RemoveTrackOrError(stream);
			}
		}

		delete pcObserver;
		result = true;
	}
	Json::Value answer;
	if (result)
	{
		answer = result;
	}
	RTC_LOG(LS_INFO) << __FUNCTION__ << " " << peerid << " result:" << result;
	return answer;
}

/* ---------------------------------------------------------------------------
**  get list ICE candidate associayed with a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceCandidateList(const std::string &peerid)
{
	RTC_LOG(LS_INFO) << __FUNCTION__;

	Json::Value value;
	std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
	std::map<std::string, PeerConnectionObserver *>::iterator it = m_peer_connectionobs_map.find(peerid);
	if (it != m_peer_connectionobs_map.end())
	{
		PeerConnectionObserver *obs = it->second;
		if (obs)
		{
			value = obs->getIceCandidateList();
		}
		else
		{
			RTC_LOG(LS_ERROR) << "No observer for peer:" << peerid;
		}
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  get PeerConnection list
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getPeerConnectionList()
{
	Json::Value value(Json::arrayValue);

	std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
	for (auto it : m_peer_connectionobs_map)
	{
		Json::Value content;

		// get local SDP
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		if ((peerConnection) && (peerConnection->local_description()))
		{
			content["pc_state"] =  std::string(webrtc::PeerConnectionInterface::AsString(peerConnection->peer_connection_state()));
			content["signaling_state"] =  std::string(webrtc::PeerConnectionInterface::AsString(peerConnection->signaling_state()));
			content["ice_state"] =  std::string(webrtc::PeerConnectionInterface::AsString(peerConnection->ice_connection_state()));			

			std::string sdp;
			peerConnection->local_description()->ToString(&sdp);
			content["sdp"] = sdp;

			content["candidateList"] = it.second->getIceCandidateList();

			Json::Value streams;
			std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> localstreams = peerConnection->GetSenders();
			for (auto localStream : localstreams)
			{
				if (localStream != NULL)
				{
					webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> mediaTrack = localStream->track();			
					if (mediaTrack) {
						Json::Value track;
						track["kind"] = mediaTrack->kind();
						if (track["kind"] == "video") {
							webrtc::VideoTrackInterface* videoTrack = (webrtc::VideoTrackInterface*)mediaTrack.get();
							webrtc::VideoTrackSourceInterface::Stats stats;
							if (videoTrack->GetSource())
							{
								track["state"] = videoTrack->GetSource()->state();
								if (videoTrack->GetSource()->GetStats(&stats))
								{
									track["width"] = stats.input_width;
									track["height"] = stats.input_height;
								}
							}							
						} else if (track["kind"] == "audio") {
							webrtc::AudioTrackInterface* audioTrack = (webrtc::AudioTrackInterface*)mediaTrack.get();
							if (audioTrack->GetSource())
							{
								track["state"] = audioTrack->GetSource()->state();
							}
							int level = 0;
							if (audioTrack->GetSignalLevel(&level)) {
								track["level"] = level;
							}							
						}

						std::string streamLabel = localStream->stream_ids()[0];		
						if (!streams.isMember(streamLabel)) {
							streams[streamLabel] = Json::Value(Json::objectValue);
						}			
						streams[streamLabel][mediaTrack->id()] = track;
					}
				}
			}
			content["streams"] = streams;
		}
		
		Json::Value pc;
		pc[it.first] = content;
		value.append(pc);
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  get StreamList list
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getStreamList()
{
	std::lock_guard<std::mutex> mlock(m_streamMapMutex);
	Json::Value value(Json::arrayValue);
	for (auto it : m_stream_map)
	{
		value.append(it.first);
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  check if factory is initialized
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::InitializePeerConnection()
{
	return (m_peer_connection_factory.get() != NULL);
}

/* ---------------------------------------------------------------------------
**  get oldest PeerConnection
** -------------------------------------------------------------------------*/
std::string PeerConnectionManager::getOldestPeerCannection()
{
	uint64_t oldestpc = std::numeric_limits<uint64_t>::max();
	std::string oldestpeerid;
	std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
	if ( (m_maxpc > 0) && (m_peer_connectionobs_map.size() >= m_maxpc) ) {
		for (auto it : m_peer_connectionobs_map) {
			uint64_t creationTime = it.second->getCreationTime();
			if (creationTime < oldestpc) {
				oldestpc = creationTime;
				oldestpeerid = it.second->getPeerId();
			}
		}
	}
	return oldestpeerid;
}

/* ---------------------------------------------------------------------------
**  create a new PeerConnection
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionObserver *PeerConnectionManager::CreatePeerConnection(const std::string &peerid)
{
	std::string oldestpeerid = this->getOldestPeerCannection();
	if (!oldestpeerid.empty()) {
		this->hangUp(oldestpeerid);
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	if (m_usePlanB) {
		config.sdp_semantics = webrtc::SdpSemantics::kPlanB;
	} else {
		config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	}
	for (auto iceServer : m_iceServerList)
	{
		webrtc::PeerConnectionInterface::IceServer server = getIceServerFromUrl(iceServer);
		config.servers.push_back(server);
	}
	config.type = m_transportType;

	// Use example From https://soru.site/questions/51578447/api-c-webrtcyi-kullanarak-peerconnection-ve-ucretsiz-baglant-noktasn-serbest-nasl
	int minPort = 0;
	int maxPort = 65535;
	std::istringstream is(m_webrtcPortRange);
	std::string port;
	if (std::getline(is, port, ':')) {
		minPort = std::stoi(port);
		if (std::getline(is, port, ':')) {
			maxPort = std::stoi(port);
		}
	}

	config.port_allocator_config.min_port = minPort;
	config.port_allocator_config.max_port = maxPort;

	RTC_LOG(LS_INFO) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid << " webrtcPortRange:" << minPort << ":" << maxPort;

	PeerConnectionObserver *obs = new PeerConnectionObserver(this, peerid, config);
	if (!obs)
	{
		RTC_LOG(LS_ERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/

webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> PeerConnectionManager::CreateVideoSource(const std::string &videourl, const std::map<std::string, std::string> &opts)
{
	RTC_LOG(LS_INFO) << "videourl:" << videourl;

	return CapturerFactory::CreateVideoSource(videourl, opts, m_publishFilter, m_peer_connection_factory, m_video_decoder_factory);
}

webrtc::scoped_refptr<webrtc::AudioSourceInterface> PeerConnectionManager::CreateAudioSource(const std::string &audiourl, const std::map<std::string, std::string> &opts)
{
	RTC_LOG(LS_INFO) << "audiourl:" << audiourl;

	return m_workerThread->BlockingCall([this, audiourl, opts] {
		return CapturerFactory::CreateAudioSource(audiourl, opts, m_publishFilter, m_peer_connection_factory, m_audioDecoderfactory, m_audioDeviceModule);
    });
}

const std::string PeerConnectionManager::sanitizeLabel(const std::string &label)
{
	std::string out(label);

	// conceal labels that contain rtsp URL to prevent sensitive data leaks.
	if (label.find("rtsp:") != std::string::npos)
	{
		std::hash<std::string> hash_fn;
		size_t hash = hash_fn(out);
		return std::to_string(hash);
	}

	out.erase(std::remove_if(out.begin(), out.end(), ignoreInLabel), out.end());
	return out;
}

/* ---------------------------------------------------------------------------
**  Add a stream to a PeerConnection
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface *peer_connection, const std::string &videourl, const std::string &audiourl, const std::string &options)
{
	bool ret = false;

	// compute options
	std::string optstring = options;
	if (m_config.isMember(videourl)) {
		std::string urlopts = m_config[videourl]["options"].asString();
		if (options.empty()) {
			optstring = urlopts;
		} else if (options.find_first_of("&")==0) {
			optstring = urlopts + options;
		} else {
			optstring = options;
		}
	}

	// convert options string into map
	std::istringstream is(optstring);
	std::map<std::string, std::string> opts;
	std::string key, value;
	while (std::getline(std::getline(is, key, '='), value, '&'))
	{
		opts[key] = value;
	}

	std::string video = videourl;
	if (m_config.isMember(video)) {
		video = m_config[video]["video"].asString();
	}

	std::string audio = audiourl;
	if (m_config.isMember(audio)) {
		audio = m_config[audio]["audio"].asString();
	}

	// set bandwidth
	if (opts.find("bitrate") != opts.end())
	{
		int bitrate = std::stoi(opts.at("bitrate"));

		webrtc::BitrateSettings bitrateParam;
		bitrateParam.min_bitrate_bps = std::optional<int>(bitrate / 2);
		bitrateParam.start_bitrate_bps = std::optional<int>(bitrate);
		bitrateParam.max_bitrate_bps = std::optional<int>(bitrate * 2);
		peer_connection->SetBitrate(bitrateParam);

		RTC_LOG(LS_WARNING) << "set bitrate:" << bitrate;
	}

	// keep capturer options (to improve!!!)
	std::string optcapturer;
	if ((video.find("rtsp://") == 0) || (audio.find("rtsp://") == 0))
	{
		if (opts.find("rtptransport") != opts.end())
		{
			optcapturer += opts["rtptransport"];
		}
		if (opts.find("timeout") != opts.end())
		{
			optcapturer += opts["timeout"];
		}
		if (opts.find("width") != opts.end())
		{
			optcapturer += opts["width"];
		}
		if (opts.find("height") != opts.end())
		{
			optcapturer += opts["height"];
		}
	}

	// compute stream label removing space because SDP use label
	std::string streamLabel = this->sanitizeLabel(videourl + "|" + audiourl + "|" + optcapturer);

	bool existingStream = false;
	{
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		existingStream = (m_stream_map.find(streamLabel) != m_stream_map.end());
	}

	if (!existingStream)
	{
		// need to create the stream
		webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource(this->CreateVideoSource(video, opts));
		webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource(this->CreateAudioSource(audio, opts));
		RTC_LOG(LS_INFO) << "Adding Stream to map";
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		m_stream_map[streamLabel] = std::make_pair(videoSource, audioSource);
	}

	// create a new webrtc stream
	{
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		std::map<std::string, std::pair<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, webrtc::scoped_refptr<webrtc::AudioSourceInterface>>>::iterator it = m_stream_map.find(streamLabel);
		if (it != m_stream_map.end())
		{
				std::pair<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, webrtc::scoped_refptr<webrtc::AudioSourceInterface>> pair = it->second;
				webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource(pair.first);
				if (!videoSource)
				{
					RTC_LOG(LS_ERROR) << "Cannot create capturer video:" << videourl;
				}
				else
				{
					webrtc::scoped_refptr<webrtc::VideoTrackInterface> video_track = m_peer_connection_factory->CreateVideoTrack(videoSource, streamLabel + "_video");
					if ((video_track) && (!peer_connection->AddTrack(video_track, {streamLabel}).ok()))
					{
						RTC_LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
					}
					else
					{
						RTC_LOG(LS_INFO) << "VideoTrack added to PeerConnection";
						ret = true;
					}					
				}

				webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource(pair.second);
				if (!audioSource)
				{
					RTC_LOG(LS_ERROR) << "Cannot create capturer audio:" << audio;
				}
				else
				{
					webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track = m_peer_connection_factory->CreateAudioTrack(streamLabel + "_audio", audioSource.get());
					if ((audio_track) && (!peer_connection->AddTrack(audio_track, {streamLabel}).ok()))
					{
						RTC_LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
					} 
					else
					{
						RTC_LOG(LS_INFO) << "AudioTrack added to PeerConnection";
						ret = true;
					}
				}
		}
		else
		{
			RTC_LOG(LS_ERROR) << "Cannot find stream";
		}
	}

	return ret;
}

/* ---------------------------------------------------------------------------
**  ICE callback
** -------------------------------------------------------------------------*/
void PeerConnectionManager::PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidate *candidate)
{
	RTC_LOG(LS_INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

	const std::string sdp = candidate->ToString();
	RTC_LOG(LS_INFO) << sdp;

	Json::Value jmessage;
	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	jmessage[kCandidateSdpName] = sdp;
	m_iceCandidateList.append(jmessage);
}

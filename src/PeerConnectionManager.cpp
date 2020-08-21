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

#include "PeerConnectionManager.h"
#include "V4l2AlsaMap.h"
#include "CapturerFactory.h"

#include "VideoScaler.h"
#include "VideoFilter.h"


#include "p2p/client/basic_port_allocator.h"


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
std::string getServerIpFromClientIp(int clientip)
{
	return "127.0.0.1";
}
#else
#include <net/if.h>
#include <ifaddrs.h>
std::string getServerIpFromClientIp(int clientip)
{
	std::string serverAddress;
	char host[NI_MAXHOST];
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
	return serverAddress;
}
#endif

struct IceServer
{
	std::string url;
	std::string user;
	std::string pass;
};

IceServer getIceServerFromUrl(const std::string &url, const std::string &clientIp = "")
{
	IceServer srv;
	srv.url = url;

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
		srv.url = protocol + ":" + uri;

		if (!credentials.empty())
		{
			pos = credentials.find(':');
			if (pos == std::string::npos)
			{
				srv.user = credentials;
			}
			else
			{
				srv.user = credentials.substr(0, pos);
				srv.pass = credentials.substr(pos + 1);
			}
		}
	}

	return srv;
}


webrtc::PeerConnectionFactoryDependencies CreatePeerConnectionFactoryDependencies(rtc::scoped_refptr<webrtc::AudioDeviceModule> audioDeviceModule, rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderfactory)
{
	webrtc::PeerConnectionFactoryDependencies dependencies;
	dependencies.network_thread = NULL;
	dependencies.worker_thread = rtc::Thread::Current();
	dependencies.signaling_thread = NULL;
	dependencies.call_factory = webrtc::CreateCallFactory();
	dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
	dependencies.event_log_factory = absl::make_unique<webrtc::RtcEventLogFactory>(dependencies.task_queue_factory.get());

	cricket::MediaEngineDependencies mediaDependencies;
	mediaDependencies.task_queue_factory = dependencies.task_queue_factory.get();
	mediaDependencies.adm = std::move(audioDeviceModule);
	mediaDependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
	mediaDependencies.audio_decoder_factory = std::move(audioDecoderfactory);
	mediaDependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();

	mediaDependencies.video_encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
	mediaDependencies.video_decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();

	dependencies.media_engine = cricket::CreateMediaEngine(std::move(mediaDependencies));

	return dependencies;
}

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionManager(const std::list<std::string> &iceServerList, const Json::Value & config, const webrtc::AudioDeviceModule::AudioLayer audioLayer, const std::string &publishFilter, const std::string & webrtcUdpPortRange)
	: m_audioDecoderfactory(webrtc::CreateBuiltinAudioDecoderFactory()), m_task_queue_factory(webrtc::CreateDefaultTaskQueueFactory()),
#ifdef HAVE_SOUND
	  m_audioDeviceModule(webrtc::AudioDeviceModule::Create(audioLayer, m_task_queue_factory.get())),
#else
	  m_audioDeviceModule(new webrtc::FakeAudioDeviceModule()),
#endif
	  m_peer_connection_factory(webrtc::CreateModularPeerConnectionFactory(CreatePeerConnectionFactoryDependencies(m_audioDeviceModule, m_audioDecoderfactory))), 
	  m_iceServerList(iceServerList), m_config(config), m_publishFilter(publishFilter)
{
	// build video audio map
	m_videoaudiomap = getV4l2AlsaMap();

	// Set the webrtc port range
	m_webrtcPortRange = webrtcUdpPortRange;

	// register api in http server
	m_func["/api/getMediaList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getMediaList();
	};

	m_func["/api/getVideoDeviceList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getVideoDeviceList();
	};

	m_func["/api/getAudioDeviceList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getAudioDeviceList();
	};

	m_func["/api/getIceServers"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getIceServers(req_info->remote_addr);
	};

	m_func["/api/call"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		std::string url;
		std::string audiourl;
		std::string options;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
			CivetServer::getParam(req_info->query_string, "url", url);
			CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
			CivetServer::getParam(req_info->query_string, "options", options);
		}
		return this->call(peerid, url, audiourl, options, in);
	};

	m_func["/api/hangup"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
		}
		return this->hangUp(peerid);
	};

	m_func["/api/createOffer"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		std::string url;
		std::string audiourl;
		std::string options;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
			CivetServer::getParam(req_info->query_string, "url", url);
			CivetServer::getParam(req_info->query_string, "audiourl", audiourl);
			CivetServer::getParam(req_info->query_string, "options", options);
		}
		return this->createOffer(peerid, url, audiourl, options);
	};
	m_func["/api/setAnswer"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
		}
		return this->setAnswer(peerid, in);
	};

	m_func["/api/getIceCandidate"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
		}
		return this->getIceCandidateList(peerid);
	};

	m_func["/api/addIceCandidate"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string peerid;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "peerid", peerid);
		}
		return this->addIceCandidate(peerid, in);
	};

	m_func["/api/getPeerConnectionList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getPeerConnectionList();
	};

	m_func["/api/getStreamList"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		return this->getStreamList();
	};

	m_func["/api/version"] = [](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		Json::Value answer(VERSION);
		return answer;
	};
	m_func["/api/log"] = [](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		std::string loglevel;
		if (req_info->query_string)
		{
			CivetServer::getParam(req_info->query_string, "level", loglevel);
			if (!loglevel.empty())
			{
				rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)atoi(loglevel.c_str()));
			}
		}
		Json::Value answer(rtc::LogMessage::GetLogToDebug());
		return answer;
	};
	m_func["/api/help"] = [this](const struct mg_request_info *req_info, const Json::Value &in) -> Json::Value {
		Json::Value answer;
		for (auto it : m_func)
		{
			answer.append(it.first);
		}
		return answer;
	};
}

/* ---------------------------------------------------------------------------
**  Destructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::~PeerConnectionManager()
{
}

/* ---------------------------------------------------------------------------
**  return deviceList as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getMediaList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> videoCaptureDevice = CapturerFactory::GetVideoCaptureDeviceList(m_publishFilter);
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

	const std::list<std::string> videoList = CapturerFactory::GetVideoSourceList(m_publishFilter);
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

	const std::list<std::string> videoCaptureDevice = CapturerFactory::GetVideoCaptureDeviceList(m_publishFilter);
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

	if (std::regex_match("audiocap://", m_publishFilter))
	{
		int16_t num_audioDevices = m_audioDeviceModule->RecordingDevices();
		RTC_LOG(INFO) << "nb audio devices:" << num_audioDevices;

		for (int i = 0; i < num_audioDevices; ++i)
		{
			char name[webrtc::kAdmMaxDeviceNameSize] = {0};
			char id[webrtc::kAdmMaxGuidSize] = {0};
			if (m_audioDeviceModule->RecordingDeviceName(i, name, id) != -1)
			{
				RTC_LOG(INFO) << "audio device name:" << name << " id:" << id;
				value.append(name);
			}
		}
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return iceServers as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceServers(const std::string &clientIp)
{
	Json::Value urls;

	for (auto iceServer : m_iceServerList)
	{
		Json::Value server;
		Json::Value urlList(Json::arrayValue);
		IceServer srv = getIceServerFromUrl(iceServer, clientIp);
		RTC_LOG(INFO) << "ICE URL:" << srv.url;
		urlList.append(srv.url);
		server["urls"] = urlList;
		if (srv.user.length() > 0)
			server["username"] = srv.user;
		if (srv.pass.length() > 0)
			server["credential"] = srv.pass;
		urls.append(server);
	}

	Json::Value iceServers;
	iceServers["iceServers"] = urls;

	return iceServers;
}

/* ---------------------------------------------------------------------------
**  get PeerConnection associated with peerid
** -------------------------------------------------------------------------*/
rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnectionManager::getPeerConnection(const std::string &peerid)
{
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
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
	if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) || !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) || !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message:" << jmessage;
	}
	else
	{
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));
		if (!candidate.get())
		{
			RTC_LOG(WARNING) << "Can't parse received candidate message.";
		}
		else
		{
			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
			if (peerConnection)
			{
				if (!peerConnection->AddIceCandidate(candidate.get()))
				{
					RTC_LOG(WARNING) << "Failed to apply the received candidate";
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
	RTC_LOG(INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options;
	Json::Value offer;

	PeerConnectionObserver *peerConnectionObserver = this->CreatePeerConnection(peerid);
	if (!peerConnectionObserver)
	{
		RTC_LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	else
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();

		if (!this->AddStreams(peerConnection, videourl, audiourl, options))
		{
			RTC_LOG(WARNING) << "Can't add stream";
		}

		// register peerid
		{
			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			m_peer_connectionobs_map.insert(std::pair<std::string, PeerConnectionObserver *>(peerid, peerConnectionObserver));
		}

		// ask to create offer
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
		rtcoptions.offer_to_receive_video = 0;
		rtcoptions.offer_to_receive_audio = 0;
		std::promise<const webrtc::SessionDescriptionInterface *> promise;
		peerConnection->CreateOffer(CreateSessionDescriptionObserver::Create(peerConnection, promise), rtcoptions);

		// waiting for offer
		std::future<const webrtc::SessionDescriptionInterface *> future = promise.get_future();
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
				RTC_LOG(LERROR) << "Failed to create offer";
			}
		}
		else
		{
			RTC_LOG(LERROR) << "Failed to create offer";
		}
	}
	return offer;
}

/* ---------------------------------------------------------------------------
** set answer to a call initiated by createOffer
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::setAnswer(const std::string &peerid, const Json::Value &jmessage)
{
	RTC_LOG(INFO) << jmessage;
	Json::Value answer;

	std::string type;
	std::string sdp;
	if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type) || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
		answer["error"] = "Can't parse received message.";
	}
	else
	{
		webrtc::SessionDescriptionInterface *session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
		if (!session_description)
		{
			RTC_LOG(WARNING) << "Can't parse received session description message.";
			answer["error"] = "Can't parse received session description message.";
		}
		else
		{
			RTC_LOG(LERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();

			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
			if (peerConnection)
			{
				std::promise<const webrtc::SessionDescriptionInterface *> remotepromise;
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection, remotepromise), session_description);
				// waiting for remote description
				std::future<const webrtc::SessionDescriptionInterface *> remotefuture = remotepromise.get_future();
				if (remotefuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
				{
					RTC_LOG(INFO) << "remote_description is ready";
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
					RTC_LOG(WARNING) << "Can't get remote description.";
					answer["error"] = "Can't get remote description.";
				}
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
	RTC_LOG(INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options;

	Json::Value answer;

	std::string type;
	std::string sdp;

	if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type) || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		PeerConnectionObserver *peerConnectionObserver = this->CreatePeerConnection(peerid);
		if (!peerConnectionObserver)
		{
			RTC_LOG(LERROR) << "Failed to initialize PeerConnectionObserver";
		}
		else if (!peerConnectionObserver->getPeerConnection().get())
		{
			RTC_LOG(LERROR) << "Failed to initialize PeerConnection";
			delete peerConnectionObserver;
		}
		else
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();
			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() << " localDescription:" << peerConnection->local_description();

			// register peerid
			{
				std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
				m_peer_connectionobs_map.insert(std::pair<std::string, PeerConnectionObserver *>(peerid, peerConnectionObserver));
			}

			// set remote offer
			webrtc::SessionDescriptionInterface *session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
			if (!session_description)
			{
				RTC_LOG(WARNING) << "Can't parse received session description message.";
			}
			else
			{
				std::promise<const webrtc::SessionDescriptionInterface *> remotepromise;
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection, remotepromise), session_description);
				// waiting for remote description
				std::future<const webrtc::SessionDescriptionInterface *> remotefuture = remotepromise.get_future();
				if (remotefuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
				{
					RTC_LOG(INFO) << "remote_description is ready";
				}
				else
				{
					RTC_LOG(WARNING) << "remote_description is NULL";
				}
			}

			// add local stream
			if (!this->AddStreams(peerConnection, videourl, audiourl, options))
			{
				RTC_LOG(WARNING) << "Can't add stream";
			}

			// create answer
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
			std::promise<const webrtc::SessionDescriptionInterface *> localpromise;
			peerConnection->CreateAnswer(CreateSessionDescriptionObserver::Create(peerConnection, localpromise), rtcoptions);

			// waiting for answer
			std::future<const webrtc::SessionDescriptionInterface *> localfuture = localpromise.get_future();
			if (localfuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready)
			{
				// answer with the created answer
				const webrtc::SessionDescriptionInterface *desc = localfuture.get();
				if (desc)
				{
					std::string sdp;
					desc->ToString(&sdp);

					answer[kSessionDescriptionTypeName] = desc->type();
					answer[kSessionDescriptionSdpName] = sdp;
				}
				else
				{
					RTC_LOG(LERROR) << "Failed to create answer";
				}
			}
			else
			{
				RTC_LOG(LERROR) << "Failed to create answer";
			}
		}
	}
	return answer;
}

bool PeerConnectionManager::streamStillUsed(const std::string &streamLabel)
{
	bool stillUsed = false;
	for (auto it : m_peer_connectionobs_map)
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams(peerConnection->local_streams());
		for (unsigned int i = 0; i < localstreams->count(); i++)
		{
			if (localstreams->at(i)->id() == streamLabel)
			{
				stillUsed = true;
				break;
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
	RTC_LOG(INFO) << __FUNCTION__ << " " << peerid;

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

		if (pcObserver)
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();

			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams(peerConnection->local_streams());
			for (unsigned int i = 0; i < localstreams->count(); i++)
			{
				auto stream = localstreams->at(i);

				std::string streamLabel = stream->id();
				bool stillUsed = this->streamStillUsed(streamLabel);
				if (!stillUsed)
				{
					RTC_LOG(LS_ERROR) << "hangUp stream is no more used " << streamLabel;
					std::lock_guard<std::mutex> mlock(m_streamMapMutex);
					std::map<std::string, std::pair<rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, rtc::scoped_refptr<webrtc::AudioSourceInterface>>>::iterator it = m_stream_map.find(streamLabel);
					if (it != m_stream_map.end())
					{
						m_stream_map.erase(it);
					}

					RTC_LOG(LS_ERROR) << "hangUp stream closed " << streamLabel;
				}

				peerConnection->RemoveStream(stream);
			}

			delete pcObserver;
			result = true;
		}
	}
	Json::Value answer;
	if (result)
	{
		answer = result;
	}
	RTC_LOG(INFO) << __FUNCTION__ << " " << peerid << " result:" << result;
	return answer;
}

/* ---------------------------------------------------------------------------
**  get list ICE candidate associayed with a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceCandidateList(const std::string &peerid)
{
	RTC_LOG(INFO) << __FUNCTION__;

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
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		if ((peerConnection) && (peerConnection->local_description()))
		{
			content["pc_state"] = (int)(peerConnection->peer_connection_state());
			content["signaling_state"] = (int)(peerConnection->signaling_state());
			content["ice_state"] = (int)(peerConnection->ice_connection_state());			

			std::string sdp;
			peerConnection->local_description()->ToString(&sdp);
			content["sdp"] = sdp;

			Json::Value streams;
			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams(peerConnection->local_streams());
			if (localstreams)
			{
				for (unsigned int i = 0; i < localstreams->count(); i++)
				{
					auto localStream = localstreams->at(i);
					if (localStream != NULL)
					{
						Json::Value tracks;

						const webrtc::VideoTrackVector &videoTracks = localStream->GetVideoTracks();
						for (unsigned int j = 0; j < videoTracks.size(); j++)
						{
							auto videoTrack = videoTracks.at(j);
							Json::Value track;
							track["kind"] = videoTrack->kind();
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

							tracks[videoTrack->id()] = track;
						}
						const webrtc::AudioTrackVector &audioTracks = localStream->GetAudioTracks();
						for (unsigned int j = 0; j < audioTracks.size(); j++)
						{
							auto audioTrack = audioTracks.at(j);
							Json::Value track;
							track["kind"] = audioTrack->kind();
							if (audioTrack->GetSource())
							{
								track["state"] = audioTrack->GetSource()->state();
							}
							int level = 0;
							if (audioTrack->GetSignalLevel(&level)) {
								track["level"] = level;
							}

							tracks[audioTrack->id()] = track;
						}

						streams[localStream->id()] = tracks;
					}
				}
			}
			content["streams"] = streams;
		}

		// get Stats
		//		content["stats"] = it.second->getStats();

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
**  create a new PeerConnection
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionObserver *PeerConnectionManager::CreatePeerConnection(const std::string &peerid)
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	for (auto iceServer : m_iceServerList)
	{
		webrtc::PeerConnectionInterface::IceServer server;
		IceServer srv = getIceServerFromUrl(iceServer);
		server.uri = srv.url;
		server.username = srv.user;
		server.password = srv.pass;
		config.servers.push_back(server);
	}

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
	std::unique_ptr<cricket::PortAllocator> port_allocator(new cricket::BasicPortAllocator(new rtc::BasicNetworkManager()));
	port_allocator->SetPortRange(minPort, maxPort);
	RTC_LOG(INFO) << __FUNCTION__ << "CreatePeerConnection webrtcPortRange:" << minPort << ":" << maxPort;

	RTC_LOG(INFO) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid;
	PeerConnectionObserver *obs = new PeerConnectionObserver(this, peerid, config, std::move(port_allocator));
	if (!obs)
	{
		RTC_LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> PeerConnectionManager::CreateVideoSource(const std::string &videourl, const std::map<std::string, std::string> &opts)
{
	RTC_LOG(INFO) << "videourl:" << videourl;

	std::string video = videourl;
	if (m_config.isMember(video)) {
		video = m_config[video]["video"].asString();
	}

	return CapturerFactory::CreateVideoSource(video, opts, m_publishFilter, m_peer_connection_factory);
}

rtc::scoped_refptr<webrtc::AudioSourceInterface> PeerConnectionManager::CreateAudioSource(const std::string &audiourl, const std::map<std::string, std::string> &opts)
{
	RTC_LOG(INFO) << "audiourl:" << audiourl;

	std::string audio = audiourl;
	if (m_config.isMember(audio)) {
		audio = m_config[audio]["audio"].asString();
	}

	std::map<std::string, std::string>::iterator it = m_videoaudiomap.find(audio);
	if (it != m_videoaudiomap.end())
	{
		audio = it->second;
	}

	return CapturerFactory::CreateAudioSource(audio, opts, m_publishFilter, m_peer_connection_factory, m_audioDecoderfactory, m_audioDeviceModule);
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

	// compute audiourl if not set
	std::string audio(audiourl);
	if (audio.empty())
	{
		audio = videourl;
	}

	// set bandwidth
	if (opts.find("bitrate") != opts.end())
	{
		int bitrate = std::stoi(opts.at("bitrate"));

		webrtc::BitrateSettings bitrateParam;
		bitrateParam.min_bitrate_bps = absl::optional<int>(bitrate / 2);
		bitrateParam.start_bitrate_bps = absl::optional<int>(bitrate);
		bitrateParam.max_bitrate_bps = absl::optional<int>(bitrate * 2);
		peer_connection->SetBitrate(bitrateParam);

		RTC_LOG(WARNING) << "set bitrate:" << bitrate;
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
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource(this->CreateVideoSource(video, opts));
		rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource(this->CreateAudioSource(audio, opts));
		RTC_LOG(INFO) << "Adding Stream to map";
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		m_stream_map[streamLabel] = std::make_pair(videoSource, audioSource);
	}

	// create a new webrtc stream
	{
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		std::map<std::string, std::pair<rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, rtc::scoped_refptr<webrtc::AudioSourceInterface>>>::iterator it = m_stream_map.find(streamLabel);
		if (it != m_stream_map.end())
		{
			rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = m_peer_connection_factory->CreateLocalMediaStream(streamLabel);
			if (!stream.get())
			{
				RTC_LOG(LS_ERROR) << "Cannot create stream";
			}
			else
			{
				std::pair<rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, rtc::scoped_refptr<webrtc::AudioSourceInterface>> pair = it->second;
				rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource(pair.first);
				rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
				if (!videoSource)
				{
					RTC_LOG(LS_ERROR) << "Cannot create capturer video:" << videourl;
				}
				else
				{
					rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoScaled = VideoFilter<VideoScaler>::Create(videoSource, opts);
					video_track = m_peer_connection_factory->CreateVideoTrack(streamLabel + "_video", videoScaled);
				}

				if ((video_track) && (!stream->AddTrack(video_track)))
				{
					RTC_LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
				}

				rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource(pair.second);
				rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
				if (!audioSource)
				{
					RTC_LOG(LS_ERROR) << "Cannot create capturer audio:" << audiourl;
				}
				else
				{
					audio_track = m_peer_connection_factory->CreateAudioTrack(streamLabel + "_audio", audioSource);
				}
				if ((audio_track) && (!stream->AddTrack(audio_track)))
				{
					RTC_LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
				}

				if (!peer_connection->AddStream(stream))
				{
					RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
				}
				else
				{
					RTC_LOG(INFO) << "stream added to PeerConnection";
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
void PeerConnectionManager::PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
	RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

	std::string sdp;
	if (!candidate->ToString(&sdp))
	{
		RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
	}
	else
	{
		RTC_LOG(INFO) << sdp;

		Json::Value jmessage;
		jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
		jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
		jmessage[kCandidateSdpName] = sdp;
		m_iceCandidateList.append(jmessage);
	}
}

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.cpp
**
** -------------------------------------------------------------------------*/
#define _WINSOCKAPI_    // For Windows: stops windows.h including winsock.h and use winsock2.h

#include <iostream>
#include <fstream>
#include <utility>
#include <functional>

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "modules/audio_device/include/fake_audio_device.h"


#include "PeerConnectionManager.h"
#include "V4l2AlsaMap.h"
#include "CapturerFactory.h"

#ifdef HAVE_LIVE555
#include "rtspaudiocapturer.h"
#endif


// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

// character to remove from url to make webrtc label
bool ignoreInLabel(char c) { 
	return c==' '||c==':'|| c=='.' || c=='/' || c=='&'; 
}

/* ---------------------------------------------------------------------------
**  helpers that should be moved somewhere else
** -------------------------------------------------------------------------*/

#ifdef WIN32
std::string getServerIpFromClientIp(int clientip) {
	return "127.0.0.1";
}
#else
#include <net/if.h>
#include <ifaddrs.h>
std::string getServerIpFromClientIp(int clientip) {
	std::string serverAddress;
	char host[NI_MAXHOST];
	struct ifaddrs *ifaddr = NULL;
	if (getifaddrs(&ifaddr) == 0) 
	{
		for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
		{
			if ( (ifa->ifa_netmask != NULL) && (ifa->ifa_netmask->sa_family == AF_INET) && (ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family == AF_INET) )  
			{
				struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
				struct sockaddr_in* mask = (struct sockaddr_in*)ifa->ifa_netmask;
				if ( (addr->sin_addr.s_addr & mask->sin_addr.s_addr) == (clientip & mask->sin_addr.s_addr) )
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

struct IceServer {
	std::string url;
	std::string user;
	std::string pass;
};

IceServer getIceServerFromUrl(const std::string & url, const std::string& clientIp = "") {
	IceServer srv;
	srv.url = url;
	
	std::size_t pos = url.find_first_of(':');
	if (pos != std::string::npos)
	{
		std::string protocol = url.substr(0, pos);
		std::string uri = url.substr(pos + 1);		
		std::string credentials;			
		
		std::size_t pos = uri.find('@');
		if (pos != std::string::npos)
		{
			credentials = uri.substr(0, pos);			
			uri = uri.substr(pos + 1);
		}
		
		if ( (uri.find("0.0.0.0:") == 0) && (clientIp.empty() == false)) {
			// answer with ip that is on same network as client
			std::string clienturl = getServerIpFromClientIp(inet_addr(clientIp.c_str()));
			clienturl += uri.substr(uri.find_first_of(':'));
			uri = clienturl;
		}
		srv.url = protocol + ":" + uri;

		if (!credentials.empty()) {
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

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionManager( const std::list<std::string> & iceServerList
					    , const std::map<std::string,std::string> & urlVideoList
					    , const std::map<std::string,std::string> & urlAudioList
					    , const webrtc::AudioDeviceModule::AudioLayer audioLayer
                                            , const std::string& publishFilter)
	: audioDeviceModule_(new webrtc::FakeAudioDeviceModule())
	, audioDecoderfactory_(webrtc::CreateBuiltinAudioDecoderFactory())
	, peer_connection_factory_(webrtc::CreateModularPeerConnectionFactory(NULL,
                                                                    rtc::Thread::Current(),
                                                                    NULL,
																	cricket::WebRtcMediaEngineFactory::Create(
          																audioDeviceModule_, webrtc::CreateBuiltinAudioEncoderFactory(), audioDecoderfactory_,
          																webrtc::CreateBuiltinVideoEncoderFactory(), webrtc::CreateBuiltinVideoDecoderFactory(), NULL,
          																webrtc::AudioProcessingBuilder().Create()),
																	webrtc::CreateCallFactory(),
																	webrtc::CreateRtcEventLogFactory()))
	, iceServerList_(iceServerList)
	, m_urlVideoList(urlVideoList)
	, m_urlAudioList(urlAudioList)
	, m_publishFilter(publishFilter)
{
	// build video audio map
	m_videoaudiomap = getV4l2AlsaMap();
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
	for (auto videoDevice : videoCaptureDevice) {
		Json::Value media;
		media["video"] = videoDevice;
		
		std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(videoDevice);
		if (it != m_videoaudiomap.end()) {
			media["audio"] = it->second;
		}				
		value.append(media);
	}

	const std::list<std::string> videoList = CapturerFactory::GetVideoSourceList(m_publishFilter);
	for (auto videoSource : videoList) {
		Json::Value media;
		media["video"] = videoSource;
		value.append(media);
	}

	for (auto url : m_urlVideoList)
	{
		Json::Value media;
		media["video"] = url.first;
		auto audioIt = m_urlAudioList.find(url.first);
		if (audioIt != m_urlAudioList.end()) 
		{
			media["audio"] = audioIt->first;			
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
	for (auto videoDevice : videoCaptureDevice) {
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

	if (std::regex_match("audiocap://",m_publishFilter)) {		
		int16_t num_audioDevices = audioDeviceModule_->RecordingDevices();
		RTC_LOG(INFO) << "nb audio devices:" << num_audioDevices;

		for (int i = 0; i < num_audioDevices; ++i)
		{
			char name[webrtc::kAdmMaxDeviceNameSize] = {0};
			char id[webrtc::kAdmMaxGuidSize] = {0};
			if (audioDeviceModule_->RecordingDeviceName(i, name, id) != -1)
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
const Json::Value PeerConnectionManager::getIceServers(const std::string& clientIp)
{
	Json::Value urls;
	
	for (auto iceServer : iceServerList_) {
		Json::Value server;
		Json::Value urlList(Json::arrayValue);
		IceServer srv = getIceServerFromUrl(iceServer, clientIp);
		RTC_LOG(INFO) << "ICE URL:" << srv.url;
		urlList.append(srv.url);
		server["urls"] = urlList;
		if (srv.user.length() > 0) server["username"] = srv.user;
		if (srv.pass.length() > 0) server["credential"] = srv.pass;
		urls.append(server);
	}	
	
	Json::Value iceServers;
	iceServers["iceServers"] = urls;

	return iceServers;
}

/* ---------------------------------------------------------------------------
**  get PeerConnection associated with peerid
** -------------------------------------------------------------------------*/
rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnectionManager::getPeerConnection(const std::string& peerid)
{
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		peerConnection = it->second->getPeerConnection();
	}
	return peerConnection;
}
/* ---------------------------------------------------------------------------
**  add ICE candidate to a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::addIceCandidate(const std::string& peerid, const Json::Value& jmessage)
{
	bool result = false;
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid)
	   || !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex)
	   || !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
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
	if (result) {
		answer = result;
	}
	return answer;
}

/* ---------------------------------------------------------------------------
** create an offer for a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options)
{
	RTC_LOG(INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options ;
	Json::Value offer;

	PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
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
			peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));
		}

		// ask to create offer
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
		peerConnection->CreateOffer(CreateSessionDescriptionObserver::Create(peerConnection), rtcoptions);

		// waiting for offer
		int count=10;
		while ( (peerConnection->local_description() == NULL) && (--count > 0) ) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));					
		}

		// answer with the created offer
		const webrtc::SessionDescriptionInterface* desc = peerConnection->local_description();
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
	return offer;
}

/* ---------------------------------------------------------------------------
** set answer to a call initiated by createOffer
** -------------------------------------------------------------------------*/
void PeerConnectionManager::setAnswer(const std::string &peerid, const Json::Value& jmessage)
{
	RTC_LOG(INFO) << jmessage;

	std::string type;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
		if (!session_description)
		{
			RTC_LOG(WARNING) << "Can't parse received session description message.";
		}
		else
		{
			RTC_LOG(LERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();

			std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = this->getPeerConnection(peerid);
			if (peerConnection)
			{
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection), session_description);
			}
		}
	}
}

/* ---------------------------------------------------------------------------
**  auto-answer to a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::call(const std::string & peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value& jmessage)
{
	RTC_LOG(INFO) << __FUNCTION__ << " video:" << videourl << " audio:" << audiourl << " options:" << options ;

	Json::Value answer;

	std::string type;
	std::string sdp;

	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
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
				peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));
			}

			// set remote offer
			webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
			if (!session_description)
			{
				RTC_LOG(WARNING) << "Can't parse received session description message.";
			}
			else
			{
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection), session_description);
			}
			
			// waiting for remote description
			int count=10;
			while ( (peerConnection->remote_description() == NULL) && (--count > 0) ) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));					
			}
			if (peerConnection->remote_description() == NULL) {
				RTC_LOG(WARNING) << "remote_description is NULL";
			}

			// add local stream
			if (!this->AddStreams(peerConnection, videourl, audiourl, options))
			{
				RTC_LOG(WARNING) << "Can't add stream";
			}

			// create answer
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcoptions;
			rtcoptions.offer_to_receive_video = 0;
			rtcoptions.offer_to_receive_audio = 0;
			peerConnection->CreateAnswer(CreateSessionDescriptionObserver::Create(peerConnection), rtcoptions);

			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count()
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();

			// waiting for answer
			count=10;
			while ( (peerConnection->local_description() == NULL) && (--count > 0) ) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));					
			}

			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count()
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();

			// return the answer
			const webrtc::SessionDescriptionInterface* desc = peerConnection->local_description();
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
	}
	return answer;
}

bool PeerConnectionManager::streamStillUsed(const std::string & streamLabel)
{
	bool stillUsed = false;
	for (auto it: peer_connectionobs_map_)
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		for (unsigned int i = 0; i<localstreams->count(); i++)
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

	PeerConnectionObserver* pcObserver = NULL;
	{
		std::lock_guard<std::mutex> peerlock(m_peerMapMutex);
		std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
		if (it != peer_connectionobs_map_.end())
		{
			pcObserver = it->second;
			RTC_LOG(LS_ERROR) << "Remove PeerConnection peerid:" << peerid;
			peer_connectionobs_map_.erase(it);
		}

		if (pcObserver)
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();

			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
			for (unsigned int i = 0; i<localstreams->count(); i++)
			{
				auto stream = localstreams->at(i);

				std::string streamLabel = stream->id();
				bool stillUsed = this->streamStillUsed(streamLabel);
				if (!stillUsed)
				{
					RTC_LOG(LS_ERROR) << "hangUp stream is no more used " << streamLabel;
					std::lock_guard<std::mutex> mlock(m_streamMapMutex);
					std::map < std::string, std::pair< rtc::scoped_refptr<webrtc::VideoTrackInterface>, rtc::scoped_refptr<webrtc::AudioTrackInterface>> >::iterator it = stream_map_.find(streamLabel);
					if (it != stream_map_.end())
					{
						stream_map_.erase(it);
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
	if (result) {
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
	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		PeerConnectionObserver* obs = it->second;
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
	for (auto it : peer_connectionobs_map_)
	{
		Json::Value content;

		// get local SDP
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		if ( (peerConnection) && (peerConnection->local_description()) ) {
			std::string sdp;
			peerConnection->local_description()->ToString(&sdp);
			content["sdp"] = sdp;

			Json::Value streams;
			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
			if (localstreams) {
				for (unsigned int i = 0; i<localstreams->count(); i++) {
					if (localstreams->at(i)) {
						Json::Value tracks;
						
						const webrtc::VideoTrackVector& videoTracks = localstreams->at(i)->GetVideoTracks();
						for (unsigned int j=0; j<videoTracks.size() ; j++)
						{
							auto videoTrack = videoTracks.at(j);
							Json::Value track;
							track["kind"] = videoTrack->kind();
							webrtc::VideoTrackSourceInterface::Stats stats;
							if (videoTrack->GetSource()) {
								track["state"] = videoTrack->GetSource()->state();
								if (videoTrack->GetSource()->GetStats(&stats)) {
									track["width"] = stats.input_width;
									track["height"] = stats.input_height;
								}
							}
							tracks[videoTrack->id()] = track;
						}
						const webrtc::AudioTrackVector& audioTracks = localstreams->at(i)->GetAudioTracks();
						for (unsigned int j=0; j<audioTracks.size() ; j++)
						{
							auto audioTrack = audioTracks.at(j);
							Json::Value track;
							track["kind"] = audioTrack->kind();
							if (audioTrack->GetSource()) {
								track["state"] = audioTrack->GetSource()->state();
							}
							tracks[audioTrack->id()] = track;
						}
						
						streams[localstreams->at(i)->id()] = tracks;
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
	for (auto it : stream_map_)
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
	return (peer_connection_factory_.get() != NULL);
}

/* ---------------------------------------------------------------------------
**  create a new PeerConnection
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionObserver* PeerConnectionManager::CreatePeerConnection(const std::string& peerid)
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.enable_dtls_srtp = true;
	for (auto iceServer : iceServerList_) {
		webrtc::PeerConnectionInterface::IceServer server;
		IceServer srv = getIceServerFromUrl(iceServer);
		server.uri = srv.url;
		server.username = srv.user;
		server.password = srv.pass;
		config.servers.push_back(server);
	}

	RTC_LOG(INFO) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid;
	PeerConnectionObserver* obs = new PeerConnectionObserver(this, peerid, config);
	if (!obs)
	{
		RTC_LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/
rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionManager::CreateVideoTrack(const std::string & videourl, const std::map<std::string,std::string> & opts)
{
	RTC_LOG(INFO) << "videourl:" << videourl;

	std::string video = videourl;
	auto videoit = m_urlVideoList.find(video);
	if (videoit != m_urlVideoList.end()) {
		video = videoit->second;
	}

	std::string label = this->sanitizeLabel(videourl + "_video");

	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource = CapturerFactory::CreateVideoSource(video, opts, m_publishFilter, peer_connection_factory_);
	if (!videoSource) {
		RTC_LOG(LS_ERROR) << "Cannot create capturer video:" << videourl;
	} else {
		video_track = peer_connection_factory_->CreateVideoTrack(label, videoSource);
	}

	return video_track;
}


rtc::scoped_refptr<webrtc::AudioTrackInterface> PeerConnectionManager::CreateAudioTrack(const std::string & audiourl, const std::map<std::string,std::string> & opts)
{
	RTC_LOG(INFO) << "audiourl:" << audiourl;

	std::string audio = audiourl;
	auto audioit = m_urlAudioList.find(audio);
	if (audioit != m_urlAudioList.end()) {
		audio = audioit->second;
	}

	std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(audio);
	if (it != m_videoaudiomap.end()) {
		audio = it->second;
	}

	rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource;
	if ( (audio.find("rtsp://") == 0) && (std::regex_match("rtsp://",m_publishFilter)) )
	{
#ifdef HAVE_LIVE555
		audioDeviceModule_->Terminate();
		audioSource = RTSPAudioSource::Create(audioDecoderfactory_, audio, opts);
#endif
	}
	else if (std::regex_match("audiocap://",m_publishFilter)) 
	{
		audioDeviceModule_->Init();
		int16_t num_audioDevices = audioDeviceModule_->RecordingDevices();
		int16_t idx_audioDevice = -1;
		for (int i = 0; i < num_audioDevices; ++i)
		{
			char name[webrtc::kAdmMaxDeviceNameSize] = {0};
			char id[webrtc::kAdmMaxGuidSize] = {0};
			if (audioDeviceModule_->RecordingDeviceName(i, name, id) != -1)
			{
				if (audio == name)
				{
					idx_audioDevice = i;
					break;
				}
			}
		}
		RTC_LOG(LS_ERROR) << "audiourl:" << audiourl << " idx_audioDevice:" << idx_audioDevice;
		if ( (idx_audioDevice >= 0) && (idx_audioDevice < num_audioDevices) )
		{
			audioDeviceModule_->SetRecordingDevice(idx_audioDevice);
			cricket::AudioOptions opt;
			audioSource = peer_connection_factory_->CreateAudioSource(opt);
		}
	}
	
	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
	if (!audioSource) {
		RTC_LOG(LS_ERROR) << "Cannot create capturer audio:" << audiourl;
	} else {
		std::string label = this->sanitizeLabel(audiourl + "_audio");
		audio_track = peer_connection_factory_->CreateAudioTrack(label, audioSource);
	}
	
	return audio_track;
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
bool PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & videourl, const std::string & audiourl, const std::string & options)
{
	bool ret = false;
		
	// compute stream label removing space because SDP use label
	std::string streamLabel = this->sanitizeLabel(videourl + "|" + audiourl + "|" + options);

	bool existingStream = false;
	{
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
	        existingStream = (stream_map_.find(streamLabel) != stream_map_.end());
	}
	
	if (!existingStream)
	{
		// compute audiourl if not set
		std::string audio(audiourl);
		if (audio.empty()) {
			audio = videourl;
		}

		// convert options string into map
		std::istringstream is(options);
		std::map<std::string,std::string> opts;
		std::string key, value;
		while(std::getline(std::getline(is, key, '='), value,'&')) {
			opts[key] = value;	
		}

		// set bandwidth
		if (opts.find("bitrate") != opts.end()) {
			int bitrate = std::stoi(opts.at("bitrate"));
			
			webrtc::PeerConnectionInterface::BitrateParameters bitrateParam;
			bitrateParam.min_bitrate_bps = absl::optional<int>(bitrate/2);
			bitrateParam.current_bitrate_bps = absl::optional<int>(bitrate);
			bitrateParam.max_bitrate_bps = absl::optional<int>(bitrate*2);
			peer_connection->SetBitrate(bitrateParam);			
			
			RTC_LOG(WARNING) << "set bitrate:" << bitrate;
		}					
		
		// need to create the stream
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(this->CreateVideoTrack(videourl, opts));
		rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(this->CreateAudioTrack(audio, opts));
		RTC_LOG(INFO) << "Adding Stream to map";
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		stream_map_[streamLabel] = std::make_pair(video_track,audio_track);

	}


	{
		std::lock_guard<std::mutex> mlock(m_streamMapMutex);
		std::map<std::string, std::pair<rtc::scoped_refptr<webrtc::VideoTrackInterface>,rtc::scoped_refptr<webrtc::AudioTrackInterface>> >::iterator it = stream_map_.find(streamLabel);
		if (it != stream_map_.end())
		{
			rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(streamLabel);
			if (!stream.get())
			{
				RTC_LOG(LS_ERROR) << "Cannot create stream";
			}
			else
			{
				std::pair < rtc::scoped_refptr<webrtc::VideoTrackInterface>, rtc::scoped_refptr<webrtc::AudioTrackInterface> > pair = it->second;
				rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(pair.first);
				if ( (video_track) && (!stream->AddTrack(video_track)) )
				{
					RTC_LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
				} 

				rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(pair.second);
				if ( (audio_track) && (!stream->AddTrack(audio_track)) )
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
void PeerConnectionManager::PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
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



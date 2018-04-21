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

#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/video_capture/video_capture_factory.h"
#include "media/engine/webrtcvideocapturerfactory.h"

#include "PeerConnectionManager.h"
#include "V4l2AlsaMap.h"
#include "CivetServer.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#include "rtspaudiocapturer.h"
#endif

#ifdef USE_X11
#include "screencapturer.h"
#endif

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";


/* ---------------------------------------------------------------------------
**  helpers that should be moved somewhere else
** -------------------------------------------------------------------------*/

#include <net/if.h>
#include <ifaddrs.h>
std::string getServerIpFromClientIp(int clientip)
{
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
		
		if (uri.find("0.0.0.0:") == 0) {
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
PeerConnectionManager::PeerConnectionManager(const std::list<std::string> & iceServerList, const std::map<std::string,std::string> & urlList, const webrtc::AudioDeviceModule::AudioLayer audioLayer)
	: audioDeviceModule_(webrtc::AudioDeviceModule::Create(0, audioLayer))
	, audioDecoderfactory_(webrtc::CreateBuiltinAudioDecoderFactory())
	, peer_connection_factory_(webrtc::CreatePeerConnectionFactory(NULL,
                                                                    rtc::Thread::Current(),
                                                                    NULL,
                                                                    audioDeviceModule_,
                                                                    webrtc::CreateBuiltinAudioEncoderFactory(),
                                                                    audioDecoderfactory_,
                                                                    NULL,
                                                                    NULL))
	, iceServerList_(iceServerList)
	, urlList_(urlList)
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

	
	const std::list<std::string> videoCaptureDevice = this->getVideoCaptureDeviceList();
	for (auto videoDevice : videoCaptureDevice) {
		Json::Value media;
		media["video"] = videoDevice;
		
		std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(videoDevice);
		if (it != m_videoaudiomap.end()) {
			media["audio"] = it->second;
		}				
		value.append(media);
	}
	

#ifdef USE_X11
	std::unique_ptr<webrtc::DesktopCapturer> capturer = webrtc::DesktopCapturer::CreateWindowCapturer(webrtc::DesktopCaptureOptions::CreateDefault());	
	if (capturer) {
		webrtc::DesktopCapturer::SourceList sourceList;
		if (capturer->GetSourceList(&sourceList)) {
			for (auto source : sourceList) {
				std::ostringstream os;
				os << "window://" << source.title;
				Json::Value media;
				media["video"] = os.str();
				value.append(media);
			}
		}
	}
	capturer = webrtc::DesktopCapturer::CreateScreenCapturer(webrtc::DesktopCaptureOptions::CreateDefault());		
	if (capturer) {
		webrtc::DesktopCapturer::SourceList sourceList;
		if (capturer->GetSourceList(&sourceList)) {
			for (auto source : sourceList) {
				std::ostringstream os;
				os << "screen://" << source.id;
				Json::Value media;
				media["video"] = os.str();
				value.append(media);
			}
		}
	}
#endif		

	for (auto url : urlList_)
	{
		Json::Value media;
		media["video"] = url.first;

		value.append(media);
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return video capture device list 
** -------------------------------------------------------------------------*/
const std::list<std::string> PeerConnectionManager::getVideoCaptureDeviceList()
{
	std::list<std::string> videoDeviceList;

	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
	if (info)
	{
		int num_videoDevices = info->NumberOfDevices();
		RTC_LOG(INFO) << "nb video devices:" << num_videoDevices;
		for (int i = 0; i < num_videoDevices; ++i)
		{
			const uint32_t kSize = 256;
			char name[kSize] = {0};
			char id[kSize] = {0};
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
			{
				RTC_LOG(INFO) << "video device name:" << name << " id:" << id;
				videoDeviceList.push_back(name);
			}
		}
	}

	return videoDeviceList;
}

/* ---------------------------------------------------------------------------
**  return video device List as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getVideoDeviceList()
{
	Json::Value value(Json::arrayValue);

	const std::list<std::string> videoCaptureDevice = this->getVideoCaptureDeviceList();
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
			std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
			if (it != peer_connectionobs_map_.end())
			{
				rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it->second->getPeerConnection();
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
	Json::Value offer;
	RTC_LOG(INFO) << __FUNCTION__;

	PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
	if (!peerConnectionObserver)
	{
		RTC_LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	else
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();
		
		// set bandwidth
		std::string tmp;
		if (CivetServer::getParam(options, "bitrate", tmp)) {
			int bitrate = std::stoi(tmp);
			
			webrtc::PeerConnectionInterface::BitrateParameters bitrateParam;
			bitrateParam.min_bitrate_bps = rtc::Optional<int>(bitrate/2);
			bitrateParam.current_bitrate_bps = rtc::Optional<int>(bitrate);
			bitrateParam.max_bitrate_bps = rtc::Optional<int>(bitrate*2);
			peerConnection->SetBitrate(bitrateParam);			
			
			RTC_LOG(WARNING) << "set bitrate:" << bitrate;
		}			
		
		if (!this->AddStreams(peerConnectionObserver->getPeerConnection(), videourl, audiourl, options))
		{
			RTC_LOG(WARNING) << "Can't add stream";
		}

		// register peerid
		peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));

		// ask to create offer
		peerConnectionObserver->getPeerConnection()->CreateOffer(CreateSessionDescriptionObserver::Create(peerConnectionObserver->getPeerConnection()), NULL);

		// waiting for offer
		int count=10;
		while ( (peerConnectionObserver->getPeerConnection()->local_description() == NULL) && (--count > 0) )
		{
			usleep(1000);
		}

		// answer with the created offer
		const webrtc::SessionDescriptionInterface* desc = peerConnectionObserver->getPeerConnection()->local_description();
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

			std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
			if (it != peer_connectionobs_map_.end())
			{
				rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it->second->getPeerConnection();
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
	RTC_LOG(INFO) << __FUNCTION__;
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
			RTC_LOG(LERROR) << "Failed to initialize PeerConnection";
		}
		else
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();
			
			// set bandwidth
			std::string tmp;
			if (CivetServer::getParam(options, "bitrate", tmp)) {
				int bitrate = std::stoi(tmp);
				
				webrtc::PeerConnectionInterface::BitrateParameters bitrateParam;
				bitrateParam.min_bitrate_bps = rtc::Optional<int>(bitrate/2);
				bitrateParam.current_bitrate_bps = rtc::Optional<int>(bitrate);
				bitrateParam.max_bitrate_bps = rtc::Optional<int>(bitrate*2);
				peerConnection->SetBitrate(bitrateParam);			
				
				RTC_LOG(WARNING) << "set bitrate:" << bitrate;
			}			
			
			
			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() << " localDescription:" << peerConnection->local_description();

			// register peerid
			peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));

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
			while ( (peerConnection->remote_description() == NULL) && (--count > 0) )
			{
				usleep(1000);
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
			webrtc::FakeConstraints constraints;
			constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, "false");
			constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, "false");
			peerConnection->CreateAnswer(CreateSessionDescriptionObserver::Create(peerConnection), &constraints);

			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count()
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();

			// waiting for answer
			count=10;
			while ( (peerConnection->local_description() == NULL) && (--count > 0) )
			{
				usleep(1000);
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

	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		RTC_LOG(LS_ERROR) << "Close PeerConnection";
		PeerConnectionObserver* pcObserver = it->second;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();
		peer_connectionobs_map_.erase(it);

		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		for (unsigned int i = 0; i<localstreams->count(); i++)
		{
			std::string streamLabel = localstreams->at(i)->id();

			bool stillUsed = this->streamStillUsed(streamLabel);
			if (!stillUsed)
			{
				RTC_LOG(LS_ERROR) << "Close PeerConnection no more used " << streamLabel;
				
				std::lock_guard<std::mutex> mlock(m_streamMapMutex);
				std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(streamLabel);
				if (it != stream_map_.end())
				{
					rtc::scoped_refptr<webrtc::MediaStreamInterface> mediaStream = it->second;
					stream_map_.erase(it);
					
					// remove video tracks
					while (mediaStream->GetVideoTracks().size() > 0)
					{
						mediaStream->RemoveTrack(mediaStream->GetVideoTracks().at(0));
					}
					// remove audio tracks
					while (mediaStream->GetAudioTracks().size() > 0)
					{
						mediaStream->RemoveTrack(mediaStream->GetAudioTracks().at(0));
					}
					mediaStream.release();
				}
			}
		}

		delete pcObserver;
		result = true;
	}
	Json::Value answer;
	if (result) {
		answer = result;
	}
	return answer;
}


/* ---------------------------------------------------------------------------
**  get list ICE candidate associayed with a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceCandidateList(const std::string &peerid)
{
	RTC_LOG(INFO) << __FUNCTION__;
	
	Json::Value value;
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
							Json::Value track;
							tracks[videoTracks.at(j)->kind()].append(videoTracks.at(j)->id());
						}
						const webrtc::AudioTrackVector& audioTracks = localstreams->at(i)->GetAudioTracks();
						for (unsigned int j=0; j<audioTracks.size() ; j++)
						{
							Json::Value track;
							tracks[audioTracks.at(j)->kind()].append(audioTracks.at(j)->id());
						}
						
						Json::Value stream;
						stream[localstreams->at(i)->id()] = tracks;
						
						streams.append(stream);						
					}
				}
			}
			content["streams"] = streams;
		}
		
		// get Stats
		content["stats"] = it.second->getStats();

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
	
	for (auto iceServer : iceServerList_) {
		webrtc::PeerConnectionInterface::IceServer server;
		IceServer srv = getIceServerFromUrl(iceServer);
		server.uri = srv.url;
		server.username = srv.user;
		server.password = srv.pass;
		config.servers.push_back(server);
	}

	webrtc::FakeConstraints constraints;
	constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

	PeerConnectionObserver* obs = new PeerConnectionObserver(this, peerid, config, constraints);
	if (!obs)
	{
		RTC_LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/
rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionManager::CreateVideoTrack(const std::string & videourl, const std::string & options)
{
	RTC_LOG(INFO) << "videourl:" << videourl << " options:" << options;
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;

	std::unique_ptr<cricket::VideoCapturer> capturer;
	if (videourl.find("rtsp://") == 0)
	{
#ifdef HAVE_LIVE555
		int timeout = 10;
		std::string tmp;
		if (CivetServer::getParam(options, "timeout", tmp)) {
			timeout = std::stoi(tmp);
		}
		std::string rtptransport;
		CivetServer::getParam(options, "rtptransport", rtptransport);
		capturer.reset(new RTSPVideoCapturer(videourl, timeout, rtptransport));
#endif
	}
	else if ( (videourl.find("screen://") == 0) || (videourl.find("window://") == 0) )
	{
#ifdef USE_X11
		capturer.reset(new ScreenCapturer(videourl));
#endif	
	}
	else
	{
		cricket::WebRtcVideoDeviceCapturerFactory factory;
		capturer = factory.Create(cricket::Device(videourl, 0));
	}

	if (!capturer)
	{
		RTC_LOG(LS_ERROR) << "Cannot create capturer video:" << videourl;
	}
	else
	{
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource = peer_connection_factory_->CreateVideoSource(std::move(capturer), NULL);
		video_track = peer_connection_factory_->CreateVideoTrack(kVideoLabel, videoSource);
	}
	return video_track;
}


rtc::scoped_refptr<webrtc::AudioTrackInterface> PeerConnectionManager::CreateAudioTrack(const std::string & audiourl, const std::string & options)
{
	RTC_LOG(INFO) << "audiourl:" << audiourl << " options:" << options;

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
	if (audiourl.find("rtsp://") == 0)
	{
#ifdef HAVE_LIVE555
		audioDeviceModule_->Terminate();
		rtc::scoped_refptr<RTSPAudioSource> audioSource = RTSPAudioSource::Create(audioDecoderfactory_, audiourl);
		audio_track = peer_connection_factory_->CreateAudioTrack(kAudioLabel, audioSource);
#endif
	}
	else
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
				if (audiourl == name)
				{
					idx_audioDevice = i;
					break;
				}
			}
		}
		RTC_LOG(LS_ERROR) << "audiourl:" << audiourl << " options:" << options << " idx_audioDevice:" << idx_audioDevice;
		if ( (idx_audioDevice >= 0) && (idx_audioDevice < num_audioDevices) )
		{
			audioDeviceModule_->SetRecordingDevice(idx_audioDevice);
			rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource = peer_connection_factory_->CreateAudioSource(NULL);
			audio_track = peer_connection_factory_->CreateAudioTrack(kAudioLabel, audioSource);
		}
	}
	return audio_track;
}
  
/* ---------------------------------------------------------------------------
**  Add a stream to a PeerConnection
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & videourl, const std::string & audiourl, const std::string & options)
{
	bool ret = false;

	// look in urlmap
	std::string video = videourl;
	auto videoit = urlList_.find(video);
	if (videoit != urlList_.end()) {
		video = videoit->second;
	}
	std::string audio = audiourl;
	auto audioit = urlList_.find(audio);
	if (audioit != urlList_.end()) {
		audio = audioit->second;
	}
		
	// compute stream label removing space because SDP use label
	std::string streamLabel = video;
	streamLabel.erase(std::remove_if(streamLabel.begin(), streamLabel.end(), isspace), streamLabel.end());

	std::lock_guard<std::mutex> mlock(m_streamMapMutex);
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(streamLabel);
	if (it == stream_map_.end())
	{
		// compute audiourl if not set
		if (audio.empty()) {
			if (video.find("rtsp://") == 0) {
				audio = video;
			} else {
				std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(video);
				if (it != m_videoaudiomap.end()) {
					audio = it->second;
				}
			}
		}

		// need to create the stream
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(this->CreateVideoTrack(video, options));
		rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(this->CreateAudioTrack(audio, options));
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(streamLabel);
		if (!stream.get())
		{
			RTC_LOG(LS_ERROR) << "Cannot create stream";
		}
		else
		{
			if ( (video_track) && (!stream->AddTrack(video_track)) )
			{
				RTC_LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
			} 

			if ( (audio_track) && (!stream->AddTrack(audio_track)) )
			{
				RTC_LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
			}

			RTC_LOG(INFO) << "Adding Stream to map";
			stream_map_[streamLabel] = stream;
		}
	}


	it = stream_map_.find(streamLabel);
	if (it != stream_map_.end())
	{
		if (!peer_connection->AddStream(it->second))
		{
			RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
		}
		else
		{
			RTC_LOG(INFO) << "stream added to PeerConnection";
			ret = true;
		}
	}
	else
	{
		RTC_LOG(LS_ERROR) << "Cannot find stream";
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
		iceCandidateList_.append(jmessage);
	}
}



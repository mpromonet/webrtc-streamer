/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** webrtc.cpp
** 
** -------------------------------------------------------------------------*/

#include <iostream>

#include <utility>

#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/devices/devicemanager.h"
#include "talk/media/base/videocapturer.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"

#include "webrtc.h"

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

PeerConnectionManager::PeerConnectionManager(const std::string & devid, const std::string & stunurl) : devid_(devid), stunurl_(stunurl)
{
	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
	if (!peer_connection_factory_.get()) {
		LOG(LERROR) << __FUNCTION__ << "Failed to initialize PeerConnectionFactory";
	}	
}

PeerConnectionManager::~PeerConnectionManager() 
{
	peer_connection_factory_ = NULL;
}

std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* > PeerConnectionManager::CreatePeerConnection() 
{
	webrtc::PeerConnectionInterface::IceServers servers;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	servers.push_back(server);
	PeerConnectionObserver* obs = PeerConnectionObserver::Create();
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection = peer_connection_factory_->CreatePeerConnection(servers,
							    NULL,
							    NULL,
							    NULL,
							    obs);
	if (!peer_connection) 
	{
		LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	else 
	{
		obs->setPeerConnection(peer_connection);
		this->AddStreams(peer_connection);
	}
	return std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* >(peer_connection, obs);
}

void PeerConnectionManager::DeletePeerConnection() 
{
}

void PeerConnectionManager::setAnswer(const std::string &peerid, const std::string& message)
{
	LOG(INFO) << message;	
	
	Json::Reader reader;
	Json::Value  jmessage;
	if (!reader.parse(message, jmessage)) {
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	std::string type;
	std::string sdp;
	if (  !GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) {
		LOG(WARNING) << "Can't parse received message.";
		return;
	}
	webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp));
	if (!session_description) {
		LOG(WARNING) << "Can't parse received session description message.";
		return;
	}
	LOG(LERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();
	std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator  it = peer_connection_map_.find(peerid);
	if (it != peer_connection_map_.end())
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc = it->second;
		pc->SetRemoteDescription(SetSessionDescriptionObserver::Create(pc, session_description->type()), session_description);
	}
}

void PeerConnectionManager::addIceCandidate(const std::string &peerid, const std::string& message)
{
	LOG(INFO) << message;	
	
	Json::Reader reader;
	Json::Value  jmessage;
	if (!reader.parse(message, jmessage)) {
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (  !GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) 
	   || !GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) 
	   || !GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
		LOG(WARNING) << "Can't parse received message.";
		return;
	}
	rtc::scoped_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp));
	if (!candidate.get()) {
		LOG(WARNING) << "Can't parse received candidate message.";
		return;
	}
	std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator  it = peer_connection_map_.find(peerid);
	if (it != peer_connection_map_.end())
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc = it->second;
		if (!pc->AddIceCandidate(candidate.get())) {
			LOG(WARNING) << "Failed to apply the received candidate";
			return;
		}		
	}	
}

cricket::VideoCapturer* PeerConnectionManager::OpenVideoCaptureDevice() 
{
	cricket::VideoCapturer* capturer = NULL;
	std::vector<cricket::Device> devs;
	cricket::Device device;
	rtc::scoped_ptr<cricket::DeviceManagerInterface> dev_manager(cricket::DeviceManagerFactory::Create());
	if (!dev_manager->Init()) 
	{
		LOG(LS_ERROR) << "Can't create device manager";
	}		
	else if (!dev_manager->GetVideoCaptureDevice(devid_, &device)) 
	{
		LOG(LS_ERROR) << "Can't enumerate get device name:" << devid_;
	}
	else
	{
		capturer = dev_manager->CreateVideoCapturer(device);
	}
	return capturer;
}

void PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection) 
{
	rtc::scoped_refptr<webrtc::VideoSourceInterface> source = peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(), NULL);
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, source));
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
	if (!stream.get())
	{
		LOG(LS_ERROR) << "Cannot create stream";
	}
	else
	{
		stream->AddTrack(video_track);
	
		if (!peer_connection->AddStream(stream)) 
		{
			LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
		}
	}
}

const std::string PeerConnectionManager::getOffer(std::string &peerid) 
{
	std::string offer;
	LOG(INFO) << __FUNCTION__;
	
	std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* > peer_connection = CreatePeerConnection();
	if (!peer_connection.first) 
	{
		LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	else
	{		
		std::ostringstream os;
		os << rand();
		peerid = os.str();		
		
		// register peerid
		peer_connection_map_.insert(std::pair<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >(peerid, peer_connection.first));			
		peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peer_connection.second));	
		
		peer_connection.first->CreateOffer(CreateSessionDescriptionObserver::Create(peer_connection.first), NULL);
				
		const webrtc::SessionDescriptionInterface* desc = peer_connection.first->local_description();
		if (desc)
		{
			Json::Value jmessage;
			jmessage[kSessionDescriptionTypeName] = desc->type();
			std::string sdp;
			desc->ToString(&sdp);
			jmessage[kSessionDescriptionSdpName] = sdp;
			
			Json::StyledWriter writer;
			offer = writer.write(jmessage);
		}
	}
	return offer;
}

const Json::Value PeerConnectionManager::getIceCandidateList(const std::string &peerid)
{
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
			LOG(LS_ERROR) << "No observer for peer:" << peerid;
		}
	}
	return value;
}

void PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	LOG(LS_ERROR) << __FUNCTION__ << " " << candidate->sdp_mline_index();
	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	std::string sdp;
	if (!candidate->ToString(&sdp)) 
	{
		LOG(LS_ERROR) << "Failed to serialize candidate";
	}
	else
	{	
		LOG(INFO) << sdp;	
		jmessage[kCandidateSdpName] = sdp;
		iceCandidateList_.append(jmessage);
	}
}



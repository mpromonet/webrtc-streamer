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

Conductor::Conductor(const std::string & devid, const std::string & stunurl) : devid_(devid), stunurl_(stunurl)
{
	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
	if (!peer_connection_factory_.get()) {
		LOG(LERROR) << __FUNCTION__ << "Failed to initialize PeerConnectionFactory";
	}	
}

Conductor::~Conductor() 
{
	ASSERT(peer_connection_.get() == NULL);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> Conductor::CreatePeerConnection() 
{
	webrtc::PeerConnectionInterface::IceServers servers;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	servers.push_back(server);
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection = peer_connection_factory_->CreatePeerConnection(servers,
							    NULL,
							    NULL,
							    NULL,
							    this);
	if (!peer_connection.get()) 
	{
		LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
		DeletePeerConnection();
	}
	else 
	{
		this->AddStreams(peer_connection.get());
		peer_connection->CreateOffer(DummyCreateSessionDescriptionObserver::Create(peer_connection), NULL);
	}
	return peer_connection;
}

void Conductor::DeletePeerConnection() 
{
	peer_connection_ = NULL;
	peer_connection_factory_ = NULL;
}

void Conductor::setAnswer(const std::string& message)
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
	LOG(LERROR) << "=======> Received session description :" << session_description->type();
	peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(peer_connection_, session_description->type()), session_description);
}

void Conductor::addIceCandidate(const std::string& message)
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
	if (!peer_connection_->AddIceCandidate(candidate.get())) {
		LOG(WARNING) << "Failed to apply the received candidate";
		return;
	}
}

void Conductor::CreateOffer() 
{
	if (peer_connection_.get()) {
		LOG(LERROR) << "We only support connecting to one peer at a time";
		return;
	}

	peer_connection_ = CreatePeerConnection();
	if (!peer_connection_.get()) {
		LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	typedef std::pair<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> > PeerConnectionPair;
	peer_connection_map_.insert(PeerConnectionPair("ID", peer_connection_));	
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice() 
{
	rtc::scoped_ptr<cricket::DeviceManagerInterface> dev_manager(cricket::DeviceManagerFactory::Create());
	if (!dev_manager->Init()) {
		LOG(LS_ERROR) << "Can't create device manager";
		return NULL;
	}
	std::vector<cricket::Device> devs;
	if (!dev_manager->GetVideoCaptureDevices(&devs)) {
		LOG(LS_ERROR) << "Can't enumerate video devices";
		return NULL;
	}
	std::vector<cricket::Device>::iterator dev_it = devs.begin();
	cricket::VideoCapturer* capturer = NULL;
	for (; dev_it != devs.end() && (capturer == NULL); ++dev_it) {
		capturer = dev_manager->CreateVideoCapturer(*dev_it);
		if (capturer != NULL)
		{
			LOG(LS_INFO) << "Capturer:" << capturer->GetId() ;
			if (devid_ != capturer->GetId())
			{
				capturer = NULL;
			}
		}
	}
	return capturer;
}

void Conductor::AddStreams(webrtc::PeerConnectionInterface* peer_connection) 
{
	rtc::scoped_refptr<webrtc::VideoSourceInterface> source = peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(), NULL);
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, source));
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
	stream->AddTrack(video_track);
	
	if (!peer_connection->AddStream(stream)) {
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}
}

const std::string Conductor::getOffer() 
{
	LOG(INFO) << __FUNCTION__;
	this->CreateOffer();
	
	const webrtc::SessionDescriptionInterface* desc = peer_connection_->local_description();
	std::string offer;
	if (desc)
	{
		Json::StyledWriter writer;
		Json::Value jmessage;
		jmessage[kSessionDescriptionTypeName] = desc->type();
		std::string sdp;
		desc->ToString(&sdp);
		jmessage[kSessionDescriptionSdpName] = sdp;
		offer = writer.write(jmessage);
	}
	return offer;
}

//
// PeerConnectionObserver implementation.
//
void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	std::string sdp;
	if (!candidate->ToString(&sdp)) {
		LOG(LS_ERROR) << "Failed to serialize candidate";
		return;
	}
	
	LOG(INFO) << sdp;	
	jmessage[kCandidateSdpName] = sdp;
	iceCandidateList_.append(jmessage);
}


void DummyCreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
	LOG(LERROR) << __PRETTY_FUNCTION__ << "============" << desc->type();
	m_pc->SetLocalDescription(DummySetSessionDescriptionObserver::Create(m_pc, desc->type()), desc);
}

void DummySetSessionDescriptionObserver::OnSuccess() {
	LOG(LERROR) << __PRETTY_FUNCTION__ << "============" << m_type;	
}

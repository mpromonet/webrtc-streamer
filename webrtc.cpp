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

std::string GetComputerName() 
{ 
	char computer_name[256];
	if (gethostname(computer_name, ARRAY_SIZE(computer_name)) != 0)
		strcpy(computer_name, "host");	
	return computer_name; 
}

std::string GetPeerConnectionString() 
{ 
	std::string url("stun:127.0.0.1:3478");
	return url; 
}

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
	public:
		static DummySetSessionDescriptionObserver* Create() {  return  new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();  }
		virtual void OnSuccess() {
			LOG(INFO) << __FUNCTION__;
		}
		virtual void OnFailure(const std::string& error) {
			LOG(INFO) << __FUNCTION__ << " " << error;
		}

	protected:
		DummySetSessionDescriptionObserver() {}
		~DummySetSessionDescriptionObserver() {}
};

Conductor::Conductor(const std::string & devid) : devid_(devid) 
{
}

Conductor::~Conductor() 
{
	ASSERT(peer_connection_.get() == NULL);
}

bool Conductor::connection_active() const 
{
	return peer_connection_.get() != NULL;
}

bool Conductor::InitializePeerConnection() 
{
	ASSERT(peer_connection_factory_.get() == NULL);
	ASSERT(peer_connection_.get() == NULL);

	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
	if (!peer_connection_factory_.get()) {
		LOG(LERROR) << __FUNCTION__ << "Failed to initialize PeerConnectionFactory";
		DeletePeerConnection();
		return false;
	}

	webrtc::PeerConnectionInterface::IceServers servers;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = GetPeerConnectionString();
	servers.push_back(server);
	peer_connection_ = peer_connection_factory_->CreatePeerConnection(servers,
							    NULL,
							    NULL,
							    NULL,
							    this);
	if (!peer_connection_.get()) {
		LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
		DeletePeerConnection();
	}
	AddStreams();
	return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
	peer_connection_ = NULL;
	active_streams_.clear();
	peer_connection_factory_ = NULL;
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

void Conductor::setAnswer(const std::string& message)
{
	LOG(INFO) << message;	
	
	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage)) {
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	std::string type;
	std::string json_object;
	GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);
	
	if (!type.empty()) {
		std::string sdp;
		if (!GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) {
			LOG(WARNING) << "Can't parse received session description message.";
			return;
		}
		webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp));
		if (!session_description) {
			LOG(WARNING) << "Can't parse received session description message.";
			return;
		}
		LOG(INFO) << " Received session description :" << message;
		peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description);
		if (session_description->type() == webrtc::SessionDescriptionInterface::kOffer) {
			peer_connection_->CreateAnswer(this, NULL);
		}
	} else {
		std::string sdp_mid;
		int sdp_mlineindex = 0;
		std::string sdp;
		if (!GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) ||
			!GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) ||
			!GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
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
		LOG(INFO) << " Received candidate :" << message;
	}
}

void Conductor::CreateOffer() 
{
	if (peer_connection_.get()) {
		LOG(LERROR) << "We only support connecting to one peer at a time";
		return;
	}

	if (InitializePeerConnection()) {
		peer_connection_->CreateOffer(this, NULL);
	} else {
		LOG(LERROR) << "Error" << "Failed to initialize PeerConnection";
	}
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

void Conductor::AddStreams() 
{
	if (active_streams_.find(kStreamLabel) != active_streams_.end())
		return;  // Already added.

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(peer_connection_factory_->CreateAudioTrack(kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(), NULL)));

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
	stream->AddTrack(audio_track);
	stream->AddTrack(video_track);
	
	if (!peer_connection_->AddStream(stream)) {
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}
	typedef std::pair<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > MediaStreamPair;
	active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) 
{
	LOG(INFO) << __FUNCTION__;
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);
	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage[kSessionDescriptionTypeName] = desc->type();
	std::string sdp;
	desc->ToString(&sdp);
	jmessage[kSessionDescriptionSdpName] = sdp;
	offer_ = writer.write(jmessage);
	LOG(INFO) << offer_;
}

void Conductor::OnFailure(const std::string& error) 
{
	LOG(LERROR) << error;
}

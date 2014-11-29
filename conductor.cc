/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>

#include <utility>

#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/devices/devicemanager.h"
#include "talk/media/base/videocapturer.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"

#include "conductor.h"

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

std::string GetPeerName() {
	std::string ret("user");
	ret += '@';
	ret += GetComputerName();
	return ret;
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

Conductor::Conductor(PeerConnectionClient* client, const std::string & devid)
  : peer_id_(-1), client_(client), devid_(devid) {
	client_->RegisterObserver(this);
}

Conductor::~Conductor() {
	ASSERT(peer_connection_.get() == NULL);
}

void Conductor::SwitchToPeerList(const Peers& peers) {
	printf("%s nb:%d\n", __FUNCTION__, peers.size());
	LOG(INFO) << __FUNCTION__;

	for (Peers::const_iterator iter = peers.begin(); iter != peers.end(); ++iter) 
	{
		if (iter->second != GetPeerName())
		{
			printf("Conductor::%s %d %s\n", __FUNCTION__,  iter->first, iter->second.c_str());
			this->ConnectToPeer(iter->first);
			break;
		}
	}
}

bool Conductor::connection_active() const {
	return peer_connection_.get() != NULL;
}

void Conductor::Close() {
	client_->SignOut();
	DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
	ASSERT(peer_connection_factory_.get() == NULL);
	ASSERT(peer_connection_.get() == NULL);

	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
	if (!peer_connection_factory_.get()) {
		this->MessageBox("Error", "Failed to initialize PeerConnectionFactory", true);
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
		this->MessageBox("Error", "CreatePeerConnection failed", true);
		DeletePeerConnection();
	}
	AddStreams();
	return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
	peer_connection_ = NULL;
	active_streams_.clear();
	local_renderer_.reset();
	peer_connection_factory_ = NULL;
	peer_id_ = -1;
}


//
// PeerConnectionObserver implementation.
//

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
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
	jmessage[kCandidateSdpName] = sdp;
	SendMessage(writer.write(jmessage));
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn() {
	printf("Conductor::%s\n", __FUNCTION__);  	
	LOG(INFO) << __FUNCTION__;
}

void Conductor::OnDisconnected() {
	LOG(INFO) << __FUNCTION__;
	DeletePeerConnection();
}

void Conductor::OnPeerConnected(int id, const std::string& name) {
	printf("Conductor::%s %s\n", __FUNCTION__, name.c_str());  	
	LOG(INFO) << __FUNCTION__;
	this->SwitchToPeerList(client_->peers());
}

void Conductor::OnPeerDisconnected(int id) {
	LOG(INFO) << __FUNCTION__;
	if (id == peer_id_) {
		LOG(INFO) << "Our peer disconnected";
		this->PostMessage(PEER_CONNECTION_CLOSED, NULL);
	} else {
		this->SwitchToPeerList(client_->peers());
	}
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message) {
	ASSERT(peer_id_ == peer_id || peer_id_ == -1);
	ASSERT(!message.empty());

	if (!peer_connection_.get()) {
		ASSERT(peer_id_ == -1);
		peer_id_ = peer_id;

		if (!InitializePeerConnection()) {
			LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
			client_->SignOut();
			return;
		}
	} else if (peer_id != peer_id_) {
		ASSERT(peer_id_ != -1);
		LOG(WARNING) << "Received a message from unknown peer while already in a conversation with a different peer.";
		return;
	}
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
		return;
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
		return;
	}
}

void Conductor::OnMessageSent(int err) {
	// Process the next pending message if any.
	this->PostMessage(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnServerConnectionFailure() {
	this->MessageBox("Error", "Failed to connect", true);
}

void Conductor::StartLogin(const std::string& server, int port) {
	printf("Conductor::%s %s isconnected:%d\n", __FUNCTION__, GetPeerName().c_str(), client_->is_connected());  
	if (client_->is_connected())
		return;
	client_->Connect(server, port, GetPeerName());
}

void Conductor::DisconnectFromServer() {
	if (client_->is_connected())
		client_->SignOut();
}

void Conductor::ConnectToPeer(int peer_id) {
	printf("Conductor::%s %d\n", __FUNCTION__, peer_id);  	
	ASSERT(peer_id_ == -1);
	ASSERT(peer_id != -1);

	if (peer_connection_.get()) {
		this->MessageBox("Error", "We only support connecting to one peer at a time", true);
		return;
	}

	if (InitializePeerConnection()) {
		peer_id_ = peer_id;
		peer_connection_->CreateOffer(this, NULL);
	} else {
		this->MessageBox("Error", "Failed to initialize PeerConnection", true);
	}
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice() {
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
			std::cout << "Capturer:" << capturer->GetId() << std::endl;
			if (devid_ != capturer->GetId())
			{
				capturer = NULL;
			}
		}
	}
	return capturer;
}

void Conductor::AddStreams() {
	if (active_streams_.find(kStreamLabel) != active_streams_.end())
		return;  // Already added.

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(peer_connection_factory_->CreateAudioTrack(kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(), NULL)));

	local_renderer_.reset(new VideoRenderer(video_track));

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

	stream->AddTrack(audio_track);
	stream->AddTrack(video_track);
	if (!peer_connection_->AddStream(stream)) {
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}
	typedef std::pair<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > MediaStreamPair;
	active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

void Conductor::PostMessage(int msg_id, void* data) {
	switch (msg_id) {
		case PEER_CONNECTION_CLOSED:
			LOG(INFO) << "PEER_CONNECTION_CLOSED";
			DeletePeerConnection();

			ASSERT(active_streams_.empty());
			if (client_->is_connected()) {
				this->SwitchToPeerList(client_->peers());
			}
		break;

		case SEND_MESSAGE_TO_PEER: {
			LOG(INFO) << "SEND_MESSAGE_TO_PEER";
			std::string* msg = reinterpret_cast<std::string*>(data);
			if (msg) {
				// For convenience, we always run the message through the queue.
				// This way we can be sure that messages are sent to the server
				// in the same order they were signaled without much hassle.
				pending_messages_.push_back(msg);
			}

			if (!pending_messages_.empty() && !client_->IsSendingMessage()) {
				msg = pending_messages_.front();
				pending_messages_.pop_front();

				if (!client_->SendToPeer(peer_id_, *msg) && peer_id_ != -1) {
					LOG(LS_ERROR) << "SendToPeer failed";
					DisconnectFromServer();
				}
				delete msg;
			}			

			if (!peer_connection_.get())
			peer_id_ = -1;

			break;
		}

		default:
		ASSERT(false);
		break;
	}
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);
	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage[kSessionDescriptionTypeName] = desc->type();
	std::string sdp;
	desc->ToString(&sdp);
	jmessage[kSessionDescriptionSdpName] = sdp;
	SendMessage(writer.write(jmessage));
}

void Conductor::OnFailure(const std::string& error) {
	LOG(LERROR) << error;
}

void Conductor::SendMessage(const std::string& json_object) {
	std::string* msg = new std::string(json_object);
	this->PostMessage(SEND_MESSAGE_TO_PEER, msg);
}

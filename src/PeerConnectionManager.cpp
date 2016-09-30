/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.cpp
** 
** -------------------------------------------------------------------------*/

#include <iostream>
#include <utility>


#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"

#include "PeerConnectionManager.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#endif
#ifdef HAVE_YUVFRAMEGENERATOR
#include "yuvvideocapturer.h"
#endif


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

PeerConnectionManager::PeerConnectionManager(const std::string & stunurl) : peer_connection_factory_(NULL), stunurl_(stunurl)
{
}

PeerConnectionManager::~PeerConnectionManager() 
{
	peer_connection_factory_ = NULL;
}


const Json::Value PeerConnectionManager::getDeviceList()
{
	Json::Value value;
		
	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
	if (info) 
	{
		int num_devices = info->NumberOfDevices();
		for (int i = 0; i < num_devices; ++i) 
		{
			const uint32_t kSize = 256;
			char name[kSize] = {0};
			char id[kSize] = {0};
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) 
			{
				value.append(name);
			}
		}
	}
		
	return value;
}  
  
bool PeerConnectionManager::InitializePeerConnection()
{
	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();
	return (peer_connection_factory_.get() != NULL);
}

std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionManager::PeerConnectionObserver* > PeerConnectionManager::CreatePeerConnection(const std::string & url) 
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	server.username = "";
	server.password = "";
	config.servers.push_back(server);

	PeerConnectionObserver* obs = PeerConnectionObserver::Create();
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection = peer_connection_factory_->CreatePeerConnection(config,
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
		if (!this->AddStreams(peer_connection, url))
		{
			peer_connection.release();
		}
	}
	return std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* >(peer_connection, obs);
}

void PeerConnectionManager::setAnswer(const std::string &peerid, const std::string& message)
{
	LOG(INFO) << message;	
	
	Json::Reader reader;
	Json::Value  jmessage;
	if (!reader.parse(message, jmessage)) 
	{
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	std::string type;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) 
	{
		LOG(WARNING) << "Can't parse received message.";
		return;
	}
	webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
	if (!session_description) 
	{
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
	if (!reader.parse(message, jmessage)) 
	{
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}
	
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) 
	   || !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) 
	   || !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) 
	{
		LOG(WARNING) << "Can't parse received message.";
		return;
	}
	std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));
	if (!candidate.get()) 
	{
		LOG(WARNING) << "Can't parse received candidate message.";
		return;
	}
	std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator  it = peer_connection_map_.find(peerid);
	if (it != peer_connection_map_.end())
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc = it->second;
		if (!pc->AddIceCandidate(candidate.get())) 
		{
			LOG(WARNING) << "Failed to apply the received candidate";
			return;
		}		
	}	
}
					    
cricket::VideoCapturer* PeerConnectionManager::OpenVideoCaptureDevice(const std::string & url) 
{
	LOG(LS_ERROR) << "url:" << url;	
	cricket::VideoCapturer* capturer = NULL;
	if (url.find("rtsp://") == 0)
	{
#ifdef HAVE_LIVE555
		capturer = new RTSPVideoCapturer(url);
#endif
	}
	else if (url == "YuvFramesGenerator")
	{
#ifdef HAVE_YUVFRAMEGENERATOR
		capturer = new YuvVideoCapturer();
#endif
	}
	else
	{
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
		if (info) 
		{
			int num_devices = info->NumberOfDevices();
			for (int i = 0; i < num_devices; ++i) 
			{
				const uint32_t kSize = 256;
				char name[kSize] = {0};
				char id[kSize] = {0};
				if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) 
				{
					if (url == name)
					{
						cricket::WebRtcVideoDeviceCapturerFactory factory;
						capturer = factory.Create(cricket::Device(name, 0));
					}
				}
			}
		}
	}
	return capturer;
}

bool PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & url) 
{
	bool ret = false;
	cricket::VideoCapturer* capturer = OpenVideoCaptureDevice(url);
	if (!capturer)
	{
		LOG(LS_ERROR) << "Cannot create capturer " << url;
	}
	else
	{
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source = peer_connection_factory_->CreateVideoSource(capturer, NULL);
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, source));
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);
		if (!stream.get())
		{
			LOG(LS_ERROR) << "Cannot create stream";
		}
		else
		{
			if (!stream->AddTrack(video_track))
			{
				LOG(LS_ERROR) << "Adding Track to PeerConnection failed";
			}		
			else if (!peer_connection->AddStream(stream)) 
			{
				LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
			}
			else
			{
				ret = true;
			}
		}
	}
	return ret;
}

const std::string PeerConnectionManager::getOffer(std::string &peerid, const std::string & url) 
{
	std::string offer;
	LOG(INFO) << __FUNCTION__;
	
	std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* > peer_connection = this->CreatePeerConnection(url);
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
		
		// waiting for offer
		int count=10;
		while ( (peer_connection.first->local_description() == NULL) && (--count > 0) )
		{
			rtc::Thread::Current()->ProcessMessages(10);
		}
				
		// answer with the created offer
		const webrtc::SessionDescriptionInterface* desc = peer_connection.first->local_description();
		if (desc)
		{
			std::string sdp;
			desc->ToString(&sdp);
			
			Json::Value jmessage;
			jmessage[kSessionDescriptionTypeName] = desc->type();
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

void PeerConnectionManager::hangUp(const std::string &peerid)
{
	LOG(INFO) << __FUNCTION__ << " " << peerid;
	std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator it = peer_connection_map_.find(peerid);
	if (it != peer_connection_map_.end())
	{
		LOG(INFO) << __FUNCTION__ << " Close PeerConnection";
		it->second->Close();
		peer_connection_map_.erase(it);
	}
	peer_connectionobs_map_.erase(peerid);
}

void PeerConnectionManager::PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
	
	std::string sdp;
	if (!candidate->ToString(&sdp)) 
	{
		LOG(LS_ERROR) << "Failed to serialize candidate";
	}
	else
	{	
		LOG(INFO) << sdp;	
		
		Json::Value jmessage;
		jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
		jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
		jmessage[kCandidateSdpName] = sdp;
		iceCandidateList_.append(jmessage);
	}
}



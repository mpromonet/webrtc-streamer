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
#include "webrtc/api/test/fakeconstraints.h"

#include "PeerConnectionManager.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
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
**  Constructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionManager(const std::string & stunurl, const std::list<std::string> & urlList) 
	: peer_connection_factory_(webrtc::CreatePeerConnectionFactory())
	, stunurl_(stunurl)
	, urlList_(urlList)
{
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
const Json::Value PeerConnectionManager::getDeviceList()
{
	Json::Value value;
		
	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
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
	
	for (std::string url : urlList_)
	{
		value.append(url);
	}
		
	return value;
}  

/* ---------------------------------------------------------------------------
**  return iceServers as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceServers()
{
	Json::Value url;
	std::string stunurl("stun:");
	stunurl += stunurl_;
	url["url"] = stunurl;

	Json::Value urls;
	urls.append(url);

	Json::Value iceServers;
	iceServers["iceServers"] = urls;

	return iceServers;
}
  
/* ---------------------------------------------------------------------------
**  add ICE candidate to a PeerConnection
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::addIceCandidate(const std::string& peerid, const Json::Value& jmessage)
{
	bool result = false;
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) 
	   || !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) 
	   || !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) 
	{
		LOG(WARNING) << "Can't parse received message:" << jmessage;
	}
	else
	{
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));
		if (!candidate.get()) 
		{
			LOG(WARNING) << "Can't parse received candidate message.";
		}
		else
		{
			std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
			if (it != peer_connectionobs_map_.end())
			{
				rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it->second->getPeerConnection();
				if (!peerConnection->AddIceCandidate(candidate.get())) 
				{
					LOG(WARNING) << "Failed to apply the received candidate";
				}
				else
				{
					result = true;
				}
			}	
		}
	}
	return result;
}

const Json::Value PeerConnectionManager::createOffer(const std::string &peerid, const std::string & url) 
{
	Json::Value offer;
	LOG(INFO) << __FUNCTION__;
	
	PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection();
	if (!peerConnectionObserver) 
	{
		LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	else
	{		
		if (!this->AddStreams(peerConnectionObserver->getPeerConnection(), url))
		{ 
			LOG(WARNING) << "Can't add stream";
		}		
		
		// register peerid
		peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));	
		
		// ask to create offer
		peerConnectionObserver->getPeerConnection()->CreateOffer(CreateSessionDescriptionObserver::Create(peerConnectionObserver->getPeerConnection()), NULL);
		
		// waiting for offer
		int count=10;
		while ( (peerConnectionObserver->getPeerConnection()->local_description() == NULL) && (--count > 0) )
		{
			rtc::Thread::Current()->ProcessMessages(10);
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
			LOG(LERROR) << "Failed to create offer";
		}
	}
	return offer;
}

void PeerConnectionManager::setAnswer(const std::string &peerid, const Json::Value& jmessage)
{
	LOG(INFO) << jmessage;	
	
	std::string type;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) 
	{
		LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
		if (!session_description) 
		{
			LOG(WARNING) << "Can't parse received session description message.";
		}
		else
		{
			LOG(LERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();
			
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
const Json::Value PeerConnectionManager::call(const std::string & peerid, const std::string &url, const Json::Value& jmessage) 
{
	LOG(INFO) << __FUNCTION__;
	Json::Value answer;
	
	std::string type;
	std::string sdp;

	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) 
	{
		LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection();
		if (!peerConnectionObserver) 
		{
			LOG(LERROR) << "Failed to initialize PeerConnection";
		}
		else
		{			
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();
			LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() << " localDescription:" << peerConnection->local_description();
			
			// register peerid
			peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));	

			
			// set remote offer				
			webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
			if (!session_description) 
			{
				LOG(WARNING) << "Can't parse received session description message.";
			}
			else 
			{										
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection), session_description);
			}

			// add local stream
			if (!this->AddStreams(peerConnection, url))
			{
				LOG(WARNING) << "Can't add stream";
			}
			else
			{		
				// create answer
				webrtc::FakeConstraints constraints;
				constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, "false");
				peerConnection->CreateAnswer(CreateSessionDescriptionObserver::Create(peerConnection), &constraints);								
				
				LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() 
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();
				
				// waiting for answer
				int count=10;
				while ( (peerConnection->local_description() == NULL) && (--count > 0) )
				{
					rtc::Thread::Current()->ProcessMessages(10);
				}
				
				LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() 
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
					LOG(LERROR) << "Failed to create answer";
				}
			}
		}				
	}
	return answer;
}

/* ---------------------------------------------------------------------------
**  hangup a call
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::hangUp(const std::string &peerid)
{
	bool result = false;
	LOG(INFO) << __FUNCTION__ << " " << peerid;
	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		LOG(INFO) << " Close PeerConnection";
		delete it->second;
		peer_connectionobs_map_.erase(it);
		result = true;		
	}
	return result;
}


/* ---------------------------------------------------------------------------
**  get list ICE candidate associayed with a PeerConnection
** -------------------------------------------------------------------------*/
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

/* ---------------------------------------------------------------------------
**  get PeerConnection list 
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getPeerConnectionList()
{
	Json::Value value;
	for (auto it : peer_connectionobs_map_) 
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		
		std::string sdp;
		peerConnection->local_description()->ToString(&sdp);
		
		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		Json::Value streams;
		for (int i = 0; i<localstreams->count(); i++)
		{
			streams.append(localstreams->at(i)->label());
		}
		Json::Value content;
		content["sdp"] = sdp;
		content["streams"] = streams;

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
	Json::Value value;
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
PeerConnectionManager::PeerConnectionObserver* PeerConnectionManager::CreatePeerConnection() 
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	server.username = "";
	server.password = "";
	config.servers.push_back(server);
	
	webrtc::FakeConstraints constraints;
	constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");
            
	PeerConnectionObserver* obs = PeerConnectionObserver::Create();
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection = peer_connection_factory_->CreatePeerConnection(config,
							    &constraints,
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
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/
cricket::VideoCapturer* PeerConnectionManager::OpenVideoCaptureDevice(const std::string & url) 
{
	LOG(INFO) << "url:" << url;	
	
	cricket::VideoCapturer* capturer = NULL;
	if (url.find("rtsp://") == 0)
	{
#ifdef HAVE_LIVE555
		capturer = new RTSPVideoCapturer(url);
#endif
	}
	else
	{
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
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

/* ---------------------------------------------------------------------------
**  Del a stream 
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::delStream(const std::string & url) 
{
	bool result = false;
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(url);
	if (it != stream_map_.end())
	{
		it->second.release();
		stream_map_.erase(it);
		result = true;
	}
	return result;
}
		
/* ---------------------------------------------------------------------------
**  Add a stream to a PeerConnection
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & url) 
{
	bool ret = false;
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(url);
	if (it == stream_map_.end())
	{
		// need to create the strem
		cricket::VideoCapturer* capturer = OpenVideoCaptureDevice(url);
		if (!capturer)
		{
			LOG(LS_ERROR) << "Cannot create capturer " << url;
		}
		else
		{
			rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source = peer_connection_factory_->CreateVideoSource(capturer, NULL);
			rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(peer_connection_factory_->CreateVideoTrack(kVideoLabel, source));
			rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(peer_connection_factory_->CreateAudioTrack(kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));
			rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(url);
			if (!stream.get())
			{
				LOG(LS_ERROR) << "Cannot create stream";
			}
			else
			{
				if (!stream->AddTrack(video_track))
				{
					LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
				}
				if (!stream->AddTrack(audio_track))
				{
					LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
				}
				else
				{
					LOG(INFO) << "Adding Stream to map";
					stream_map_[url] = stream;
				}
			}
		}
		
	}
	
	
	it = stream_map_.find(url);
	if (it != stream_map_.end())
	{
		if (!peer_connection->AddStream(it->second)) 
		{
			LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
		}
		else 
		{
			LOG(INFO) << "stream added to PeerConnection";
			ret = true;
		}
	}
	else
	{
		LOG(LS_ERROR) << "Cannot find stream";
	}
	
	return ret;
}

/* ---------------------------------------------------------------------------
**  ICE callback
** -------------------------------------------------------------------------*/
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



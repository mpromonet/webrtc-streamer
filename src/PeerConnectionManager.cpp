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

#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

#include "PeerConnectionManager.h"
#include "V4l2AlsaMap.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#include "CivetServer.h"
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
PeerConnectionManager::PeerConnectionManager(const std::string & stunurl, const std::string & turnurl, const std::list<std::string> & urlList)
	: audioDeviceModule_(webrtc::AudioDeviceModule::Create(0, webrtc::AudioDeviceModule::kPlatformDefaultAudio))
	, audioDecoderfactory_(webrtc::CreateBuiltinAudioDecoderFactory())
	, peer_connection_factory_(webrtc::CreatePeerConnectionFactory(NULL,
                                                                    rtc::Thread::Current(),
                                                                    NULL,
                                                                    audioDeviceModule_,
                                                                    webrtc::CreateBuiltinAudioEncoderFactory(),
                                                                    audioDecoderfactory_,
                                                                    NULL,
                                                                    NULL))
	, stunurl_(stunurl)
	, turnurl_(turnurl)
	, urlList_(urlList)
{
	if (turnurl_.length() > 0)
	{
		std::size_t pos = turnurl_.find('@');
		if (pos != std::string::npos)
		{
			std::string credentials = turnurl_.substr(0, pos);
			pos = credentials.find(':');
			if (pos == std::string::npos)
			{
				turnuser_ = credentials;
			}
			else
			{
				turnuser_ = credentials.substr(0, pos);
				turnpass_ = credentials.substr(pos + 1);
			}
			turnurl_ = turnurl_.substr(pos + 1);
		}
	}

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

	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
	if (info)
	{
		int num_videoDevices = info->NumberOfDevices();
		LOG(INFO) << "nb video devices:" << num_videoDevices;
		for (int i = 0; i < num_videoDevices; ++i)
		{
			const uint32_t kSize = 256;
			char name[kSize] = {0};
			char id[kSize] = {0};
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
			{
				LOG(INFO) << "video device name:" << name << " id:" << id;
				Json::Value media;
				media["video"] = name;
				
				std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(name);
				if (it != m_videoaudiomap.end()) {
					media["audio"] = it->second;
				}				
				value.append(media);
			}
		}
	}

	for (std::string url : urlList_)
	{
		Json::Value media;
		media["video"] = url;

		value.append(media);
	}

	return value;
}

/* ---------------------------------------------------------------------------
**  return deviceList as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getVideoDeviceList()
{
	Json::Value value(Json::arrayValue);

	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
	if (info)
	{
		int num_videoDevices = info->NumberOfDevices();
		LOG(INFO) << "nb video devices:" << num_videoDevices;
		for (int i = 0; i < num_videoDevices; ++i)
		{
			const uint32_t kSize = 256;
			char name[kSize] = {0};
			char id[kSize] = {0};
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
			{
				LOG(INFO) << "video device name:" << name << " id:" << id;
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
**  return deviceList as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getAudioDeviceList()
{
	Json::Value value(Json::arrayValue);

	int16_t num_audioDevices = audioDeviceModule_->RecordingDevices();
	LOG(INFO) << "nb audio devices:" << num_audioDevices;

	std::map<std::string,std::string> deviceMap;
	for (int i = 0; i < num_audioDevices; ++i)
	{
		char name[webrtc::kAdmMaxDeviceNameSize] = {0};
		char id[webrtc::kAdmMaxGuidSize] = {0};
		if (audioDeviceModule_->RecordingDeviceName(i, name, id) != -1)
		{
			LOG(INFO) << "audio device name:" << name << " id:" << id;
			deviceMap[name]=id;
		}
	}
	for (auto& pair : deviceMap) {
		value.append(pair.first);
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

	if (turnurl_.length() > 0)
	{
		Json::Value turn;
		turn["url"] = "turn:" + turnurl_;
		if (turnuser_.length() > 0) turn["username"] = turnuser_;
		if (turnpass_.length() > 0) turn["credential"] = turnpass_;
		urls.append(turn);
	}

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

/* ---------------------------------------------------------------------------
** create an offer for a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options)
{
	Json::Value offer;
	LOG(INFO) << __FUNCTION__;

	PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
	if (!peerConnectionObserver)
	{
		LOG(LERROR) << "Failed to initialize PeerConnection";
	}
	else
	{
		peerConnectionObserver->createDataChannel("JanusDataChannel");

		if (!this->AddStreams(peerConnectionObserver->getPeerConnection(), videourl, audiourl, options))
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

/* ---------------------------------------------------------------------------
** set answer to a call initiated by createOffer
** -------------------------------------------------------------------------*/
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
const Json::Value PeerConnectionManager::call(const std::string & peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value& jmessage)
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
		PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
		if (!peerConnectionObserver)
		{
			LOG(LERROR) << "Failed to initialize PeerConnection";
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
				
				LOG(WARNING) << "set bitrate:" << bitrate;
			}			
			
			
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
			if (!this->AddStreams(peerConnection, videourl, audiourl, options))
			{
				LOG(WARNING) << "Can't add stream";
			}
			else
			{
				// create answer
				webrtc::FakeConstraints constraints;
				constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, "false");
				constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, "false");
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

bool PeerConnectionManager::streamStillUsed(const std::string & streamLabel)
{
	bool stillUsed = false;
	for (auto it: peer_connectionobs_map_)
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		for (unsigned int i = 0; i<localstreams->count(); i++)
		{
			if (localstreams->at(i)->label() == streamLabel)
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
bool PeerConnectionManager::hangUp(const std::string &peerid)
{
	bool result = false;
	LOG(INFO) << __FUNCTION__ << " " << peerid;

	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		LOG(LS_ERROR) << "Close PeerConnection";
		PeerConnectionObserver* pcObserver = it->second;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();
		peer_connectionobs_map_.erase(it);

		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		Json::Value streams;
		for (unsigned int i = 0; i<localstreams->count(); i++)
		{
			std::string streamLabel = localstreams->at(i)->label();

			bool stillUsed = this->streamStillUsed(streamLabel);
			if (!stillUsed)
			{
				LOG(LS_ERROR) << "Close PeerConnection no more used " << streamLabel;
				std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(streamLabel);
				if (it != stream_map_.end())
				{
					// remove video tracks
					while (it->second->GetVideoTracks().size() > 0)
					{
						it->second->RemoveTrack(it->second->GetVideoTracks().at(0));
					}
					// remove audio tracks
					while (it->second->GetAudioTracks().size() > 0)
					{
						it->second->RemoveTrack(it->second->GetAudioTracks().at(0));
					}

					it->second.release();
					stream_map_.erase(it);
				}
			}
		}

		delete pcObserver;
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
						streams.append(localstreams->at(i)->label());
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
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	server.username = "";
	server.password = "";
	config.servers.push_back(server);

	if (turnurl_.length() > 0)
	{
		webrtc::PeerConnectionInterface::IceServer turnserver;
		turnserver.uri = "turn:" + turnurl_;
		turnserver.username = turnuser_;
		turnserver.password = turnpass_;
		config.servers.push_back(turnserver);
	}

	webrtc::FakeConstraints constraints;
	constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

	PeerConnectionObserver* obs = new PeerConnectionObserver(this, peerid, config, constraints);
	if (!obs)
	{
		LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}

/* ---------------------------------------------------------------------------
**  get the capturer from its URL
** -------------------------------------------------------------------------*/
rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionManager::CreateVideoTrack(const std::string & videourl, const std::string & options)
{
	LOG(INFO) << "videourl:" << videourl << " options:" << options;
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
					if (videourl == name)
					{
						cricket::WebRtcVideoDeviceCapturerFactory factory;
						capturer = factory.Create(cricket::Device(name, 0));
						break;
					}
				}
			}
		}

	}

	if (!capturer)
	{
		LOG(LS_ERROR) << "Cannot create capturer video:" << videourl;
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
	LOG(INFO) << "audiourl:" << audiourl << " options:" << options;

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
		LOG(LS_ERROR) << "audiourl:" << audiourl << " options:" << options << " idx_audioDevice:" << idx_audioDevice;
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

	// compute stream label removing space because SDP use label
	std::string streamLabel = videourl;
	streamLabel.erase(std::remove_if(streamLabel.begin(), streamLabel.end(), isspace), streamLabel.end());

	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(streamLabel);
	if (it == stream_map_.end())
	{
		// compute audiourl if not set
		std::string audio = audiourl;
		if (audio.empty()) {
			if (videourl.find("rtsp://") == 0) {
				audio = videourl;
			} else {
				std::map<std::string,std::string>::iterator it = m_videoaudiomap.find(videourl);
				if (it != m_videoaudiomap.end()) {
					audio = it->second;
				}
			}
		}

		// need to create the stream
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(this->CreateVideoTrack(videourl, options));
		rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(this->CreateAudioTrack(audio, options));
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(streamLabel);
		if (!stream.get())
		{
			LOG(LS_ERROR) << "Cannot create stream";
		}
		else
		{
			if ( (video_track) && (!stream->AddTrack(video_track)) )
			{
				LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
			}

			if ( (audio_track) && (!stream->AddTrack(audio_track)) )
			{
				LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
			}

			LOG(INFO) << "Adding Stream to map";
			stream_map_[streamLabel] = stream;
		}
	}


	it = stream_map_.find(streamLabel);
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



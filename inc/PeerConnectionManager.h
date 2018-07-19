/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <string>
#include <mutex>
#include <regex>

#include "api/peerconnectioninterface.h"
#include "api/test/fakeconstraints.h"

#include "modules/audio_device/include/audio_device.h"

#include "rtc_base/logging.h"
#include "rtc_base/json.h"
#include "VNCVideoCapturer.h"

extern const char kVNCVideoLabel[];

class PeerConnectionManager {
	class VideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
		public:
			VideoSink(webrtc::VideoTrackInterface* track): m_track(track) {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " track:" << m_track->id();
				m_track->AddOrUpdateSink(this, rtc::VideoSinkWants());
			}
			virtual ~VideoSink() {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " track:" << m_track->id();
				m_track->RemoveSink(this);
			}		

			// VideoSinkInterface implementation
			virtual void OnFrame(const webrtc::VideoFrame& video_frame) {
				rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(video_frame.video_frame_buffer()->ToI420());
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " frame:" << buffer->width() << "x" << buffer->height();
			}

		protected:
			rtc::scoped_refptr<webrtc::VideoTrackInterface> m_track;
	};
	
	class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
		public:
			static SetSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<SetSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess()
			{
				std::string sdp;
				if (m_pc->local_description())
				{
					m_pc->local_description()->ToString(&sdp);
					RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " Local SDP:" << sdp;
				}
				if (m_pc->remote_description())
				{
					m_pc->remote_description()->ToString(&sdp);
					RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " Remote SDP:" << sdp;
				}
			}
			virtual void OnFailure(const std::string& error)
			{
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			SetSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
		public:
			static CreateSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<CreateSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
			{
				std::string sdp;
				desc->ToString(&sdp);
				RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " type:" << desc->type() << " sdp:" << sdp;
				m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc), desc);
			}
			virtual void OnFailure(const std::string& error) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			CreateSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class PeerConnectionStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback {
		public:
			PeerConnectionStatsCollectorCallback() {}
			void clearReport() { m_report.clear(); }
			Json::Value getReport() { return m_report; }

		protected:
			virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
				for (const webrtc::RTCStats& stats : *report) {
					Json::Value statsMembers;
					for (const webrtc::RTCStatsMemberInterface* member : stats.Members()) {
						statsMembers[member->name()] = member->ValueToString();
					}
					m_report[stats.id()] = statsMembers;
				}
			}

			Json::Value m_report;
	};

	class DataChannelObserver : public webrtc::DataChannelObserver  {
		public:
			DataChannelObserver(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel, const std::string &peerid, PeerConnectionManager* pm): m_dataChannel(dataChannel), m_peerid(peerid), m_manager(pm) {
				m_dataChannel->RegisterObserver(this);
			
			}
			virtual ~DataChannelObserver() {
				m_dataChannel->UnregisterObserver();
			}

			// DataChannelObserver interface
			virtual void OnStateChange() {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " state:"<< webrtc::DataChannelInterface::DataStateString(m_dataChannel->state());
				std::string msg(m_dataChannel->label() + " " + webrtc::DataChannelInterface::DataStateString(m_dataChannel->state()));
				webrtc::DataBuffer buffer(msg);
				m_dataChannel->Send(buffer);
			}
			virtual void OnMessage(const webrtc::DataBuffer& buffer) {
				std::string msg((const char*)buffer.data.data(),buffer.data.size());
				RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Got back Data Channel message!!" << msg;
				std::string videoUrl = m_manager->videourl_peer_map_[m_peerid];
				if (!videoUrl.empty()) {
					RTC_LOG(LERROR) << "This stream has no video url mapping!! : " << m_peerid;
					return;
				}
				VNCVideoCapturer* capturer = m_manager->vnc_map_[videoUrl];
				if (!capturer) {
					RTC_LOG(LERROR) << "This url has no video stream!! : " << videoUrl << " : " << m_peerid;
					return;
				}
				RTC_LOG(LS_VERBOSE) << "Got VNC STream for:" << videoUrl;

				Json::Value  jmessage;
				// parse in
				Json::Reader reader;
				if (!reader.parse(msg, jmessage))
				{
					RTC_LOG(LERROR) << "Received non-json message:" << msg;
					return;
				}

				std::vector<Json::Value> events;

				RTC_LOG(LS_VERBOSE) << "Got JSON properly!!";
				if (!jmessage["events"] || !rtc::JsonArrayToValueVector(jmessage["events"], &events)) {
					RTC_LOG(LERROR) << "msg didnt contain any events:" << msg;
					return;
				}
				RTC_LOG(LS_VERBOSE) << "Got back events: " << events.size();

				for (auto &event : events) {
					int x, y, buttonMask;
					unsigned int code;
					bool down, isPress, isClick;
					if (rtc::GetBoolFromJsonObject(event, "isPress", &isPress) && isPress) {
						RTC_LOG(LERROR) << "Processing a press!!! - " << code;
						if (!rtc::GetBoolFromJsonObject(event, "down", &down)
							|| !rtc::GetUIntFromJsonObject(event, "code", &code)
						) {
							RTC_LOG(LERROR) << "Can not parse presses - " << msg;
							continue;
						}

						capturer->onPress(code, down);
						continue;
					}

					if (rtc::GetBoolFromJsonObject(event, "isClick", &isClick) && isClick) {
						if (!rtc::GetIntFromJsonObject(event, "x", &x)
							|| !rtc::GetIntFromJsonObject(event, "y", &y)
							|| !rtc::GetIntFromJsonObject(event, "button", &buttonMask)
						) {
							RTC_LOG(LERROR) << "Can not parse clicks!! - " << msg;
							continue;
						}
						RTC_LOG(LERROR) << "Processing a click!!!" << x << ":" << y;

						capturer->onClick(x, y, buttonMask);
						continue;
					}

					RTC_LOG(LERROR) << "Skipping unknown msg: " << msg;
				}
			}

		protected:
			rtc::scoped_refptr<webrtc::DataChannelInterface>    m_dataChannel;
			PeerConnectionManager* m_manager;
			const std::string &m_peerid;
	};

	class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
		public:
			PeerConnectionObserver(PeerConnectionManager* peerConnectionManager, const std::string& peerid, const webrtc::PeerConnectionInterface::RTCConfiguration & config, const webrtc::FakeConstraints & constraints)
			: m_peerConnectionManager(peerConnectionManager)
			, m_peerid(peerid)
			, m_localChannel(NULL)
			, m_remoteChannel(NULL)
			, iceCandidateList_(Json::arrayValue) {
				m_pc = m_peerConnectionManager->peer_connection_factory_->CreatePeerConnection(config,
							    &constraints,
							    NULL,
							    NULL,
							    this);

				m_statsCallback = new rtc::RefCountedObject<PeerConnectionStatsCollectorCallback>();
				
				if (m_pc.get()) {
					rtc::scoped_refptr<webrtc::DataChannelInterface>   channel = m_pc->CreateDataChannel("ServerDataChannel", NULL);
          m_localChannel = new DataChannelObserver(channel, m_peerid, m_peerConnectionManager);
				}
			};

			virtual ~PeerConnectionObserver() {
				RTC_LOG(INFO) << __PRETTY_FUNCTION__;
				delete m_localChannel;
				delete m_remoteChannel;
				if (m_pc.get()) {
					m_pc->Close();
				}
			}

			Json::Value getIceCandidateList() { return iceCandidateList_; 	}
			
			Json::Value getStats() {
				m_statsCallback->clearReport();
				m_pc->GetStats(m_statsCallback);
				int count=10;
				while ( (m_statsCallback->getReport().empty()) && (--count > 0) )
				{
					usleep(1000);
				}
				return Json::Value(m_statsCallback->getReport());
			};

			rtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };

			// PeerConnectionObserver interface
			virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)    {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " nb video tracks:" << stream->GetVideoTracks().size();
				webrtc::VideoTrackVector videoTracks = stream->GetVideoTracks();
				if (videoTracks.size()>0) {					
					m_videosink.reset(new VideoSink(videoTracks.at(0)));
				}
			}
			virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__;
			}
			virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__;
				m_remoteChannel = new DataChannelObserver(channel, m_peerid, m_peerConnectionManager);
			}
			virtual void OnRenegotiationNeeded()                              {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " peerid:" << m_peerid;;
			}

			virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
			
			virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " state:" << state << " peerid:" << m_peerid;				
			}
			virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " state:" << state  << " peerid:" << m_peerid;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{
					iceCandidateList_.clear();
					m_peerConnectionManager->hangUp(m_peerid);
				}
			}
			
			virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) {
			}


		private:
			PeerConnectionManager* m_peerConnectionManager;
			const std::string m_peerid;
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
			DataChannelObserver*    m_localChannel;
			DataChannelObserver*    m_remoteChannel;
			Json::Value iceCandidateList_;
			rtc::scoped_refptr<PeerConnectionStatsCollectorCallback> m_statsCallback;
			std::unique_ptr<VideoSink>                               m_videosink;
	};

	public:
		PeerConnectionManager(const std::list<std::string> & iceServerList, const std::map<std::string,std::string> & urlVideoList, const std::map<std::string,std::string> & urlAudioList, const webrtc::AudioDeviceModule::AudioLayer audioLayer, const std::string& publishFilter);
		virtual ~PeerConnectionManager();

		bool InitializePeerConnection();

		const Json::Value getIceCandidateList(const std::string &peerid);
		const Json::Value addIceCandidate(const std::string &peerid, const Json::Value& jmessage);
		const Json::Value getVideoDeviceList();
		const Json::Value getAudioDeviceList();
		const Json::Value getMediaList();
		const Json::Value hangUp(const std::string &peerid);
		const Json::Value call(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value& jmessage);
		const Json::Value getIceServers(const std::string& clientIp);
		const Json::Value getPeerConnectionList();
		const Json::Value getStreamList();
		const Json::Value createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options);
		void              setAnswer(const std::string &peerid, const Json::Value& jmessage);


	protected:
		PeerConnectionObserver*                 CreatePeerConnection(const std::string& peerid);
		bool                                    AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & videourl, const std::string & audiourl, const std::string & peer_id, const std::string & options);
		rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(const std::string & videourl, const std::string & peerid, const std::map<std::string,std::string> & opts);
		rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(const std::string & audiourl, const std::map<std::string,std::string> & opts);
		bool                                    streamStillUsed(const std::string & streamLabel);
		const std::list<std::string>            getVideoCaptureDeviceList();

	protected:
		rtc::scoped_refptr<webrtc::AudioDeviceModule>                             audioDeviceModule_;
		rtc::scoped_refptr<webrtc::AudioDecoderFactory>                           audioDecoderfactory_;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>                peer_connection_factory_;
		std::map<std::string, PeerConnectionObserver* >                           peer_connectionobs_map_;
		std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  stream_map_;
		std::map<std::string, VNCVideoCapturer* >  vnc_map_;
		std::map<std::string, std::string>  videourl_peer_map_;
	        std::mutex                                                                m_streamMapMutex;
		std::list<std::string>                                                              iceServerList_;
		const std::map<std::string,std::string>                                   m_urlVideoList;
		const std::map<std::string,std::string>                                   m_urlAudioList;
		std::map<std::string,std::string>                                         m_videoaudiomap;
		const std::regex                                                          m_publishFilter;
};


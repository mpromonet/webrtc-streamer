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
#include <thread>
#include <future>

#include "api/peer_connection_interface.h"
#include "api/video_codecs/video_decoder_factory.h"

#include "p2p/client/basic_port_allocator.h"

#include "modules/audio_device/include/audio_device.h"

#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/time_utils.h"

#include "HttpServerRequestHandler.h"

class PeerConnectionManager {
	class VideoSink : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
		public:
			VideoSink(const webrtc::scoped_refptr<webrtc::VideoTrackInterface> & track): m_track(track) {
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " videotrack:" << m_track->id();
				m_track->AddOrUpdateSink(this, webrtc::VideoSinkWants());
			}
			virtual ~VideoSink() {
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " videotrack:" << m_track->id();
				m_track->RemoveSink(this);
			}		

			// VideoSinkInterface implementation
			virtual void OnFrame(const webrtc::VideoFrame& video_frame) {
				webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer(video_frame.video_frame_buffer()->ToI420());
				RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << " frame:" << buffer->width() << "x" << buffer->height();
			}

		protected:
			webrtc::scoped_refptr<webrtc::VideoTrackInterface> m_track;
	};

	class AudioSink : public webrtc::AudioTrackSinkInterface {
		public:
			AudioSink(const webrtc::scoped_refptr<webrtc::AudioTrackInterface> & track): m_track(track) {
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " audiotrack:" << m_track->id();
				m_track->AddSink(this);
			}
			virtual ~AudioSink() {
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " audiotrack:" << m_track->id();
				m_track->RemoveSink(this);
			}		

			virtual void OnData(const void* audio_data,
								int bits_per_sample,
								int sample_rate,
								size_t number_of_channels,
								size_t number_of_frames) {
				RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "size:" << bits_per_sample << " format:" << sample_rate << "/" << number_of_channels << "/" << number_of_frames;
			}

		protected:
			webrtc::scoped_refptr<webrtc::AudioTrackInterface> m_track;
	};
	
	class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
		public:
			static SetSessionDescriptionObserver* Create(const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc, std::promise<const webrtc::SessionDescriptionInterface*> & promise)
			{
				return  new webrtc::RefCountedObject<SetSessionDescriptionObserver>(pc, promise);
			}
			virtual void OnSuccess()
			{
				std::string sdp;
				if (!m_cancelled) {
					if (m_pc->local_description())
					{
						m_promise.set_value(m_pc->local_description());
						m_pc->local_description()->ToString(&sdp);
						RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " Local SDP:" << sdp;
					}
					else if (m_pc->remote_description())
					{
						m_promise.set_value(m_pc->remote_description());
						m_pc->remote_description()->ToString(&sdp);
						RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " Remote SDP:" << sdp;
					}
				}
			}
			virtual void OnFailure(webrtc::RTCError error)
			{
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " " << error.message();
				if (!m_cancelled) {
					m_promise.set_value(NULL);
				}
			}
			void cancel() {
				m_cancelled = true;
			}			
		protected:
			SetSessionDescriptionObserver(const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc, std::promise<const webrtc::SessionDescriptionInterface*> & promise) : m_pc(pc), m_promise(promise), m_cancelled(false) {};

		private:
			webrtc::scoped_refptr<webrtc::PeerConnectionInterface>        m_pc;
			std::promise<const webrtc::SessionDescriptionInterface*> & m_promise;	
			bool                                                       m_cancelled;				
	};

	class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
		public:
			static CreateSessionDescriptionObserver* Create(const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc, std::promise<const webrtc::SessionDescriptionInterface*> & promise)
			{
				return new webrtc::RefCountedObject<CreateSessionDescriptionObserver>(pc,promise);
			}
			virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
			{
				std::string sdp;
				desc->ToString(&sdp);
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__ << " type:" << desc->type() << " sdp:" << sdp;
				if (!m_cancelled) {
					m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc, m_promise), desc);
				}
			}
			virtual void OnFailure(webrtc::RTCError error) {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " " << error.message();
				if (!m_cancelled) {
					m_promise.set_value(NULL);
				}
			}
			void cancel() {
				m_cancelled = true;
			}
		protected:
			CreateSessionDescriptionObserver(const webrtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc, std::promise<const webrtc::SessionDescriptionInterface*> & promise) : m_pc(pc), m_promise(promise), m_cancelled(false) {};

		private:
			webrtc::scoped_refptr<webrtc::PeerConnectionInterface>        m_pc;
			std::promise<const webrtc::SessionDescriptionInterface*> & m_promise;
			bool                                                       m_cancelled;
	};

	class PeerConnectionStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback {
		public:
			PeerConnectionStatsCollectorCallback() {}
			void clearReport() { m_report.clear(); }
			Json::Value getReport() { return m_report; }

		protected:
			virtual void OnStatsDelivered(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
				for (const webrtc::RTCStats& stats : *report) {
					Json::Value statsMembers;
					for (auto & attribute : stats.Attributes()) {
						statsMembers[attribute.name()] = attribute.ToString();
					}
					m_report[stats.id()] = statsMembers;
				}
			}

			Json::Value m_report;
	};

	class DataChannelObserver : public webrtc::DataChannelObserver  {
		public:
			DataChannelObserver(webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel): m_dataChannel(dataChannel) {
				m_dataChannel->RegisterObserver(this);
			
			}
			virtual ~DataChannelObserver() {
				m_dataChannel->UnregisterObserver();
			}

			// DataChannelObserver interface
			virtual void OnStateChange() {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " state:"<< webrtc::DataChannelInterface::DataStateString(m_dataChannel->state());
				std::string msg(m_dataChannel->label() + " " + webrtc::DataChannelInterface::DataStateString(m_dataChannel->state()));
				webrtc::DataBuffer buffer(msg);
				m_dataChannel->Send(buffer);
			}
			virtual void OnMessage(const webrtc::DataBuffer& buffer) {
				std::string msg((const char*)buffer.data.data(),buffer.data.size());
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " msg:" << msg;
			}

		protected:
			webrtc::scoped_refptr<webrtc::DataChannelInterface>    m_dataChannel;
	};

	class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
		public:
			PeerConnectionObserver(PeerConnectionManager* peerConnectionManager, const std::string& peerid, const webrtc::PeerConnectionInterface::RTCConfiguration & config)
			: m_peerConnectionManager(peerConnectionManager)
			, m_peerid(peerid)
			, m_iceCandidateList(Json::arrayValue)
			, m_deleting(false)
			, m_creationTime(webrtc::TimeMicros()) {

				RTC_LOG(LS_INFO) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid;
				webrtc::PeerConnectionDependencies dependencies(this);

				webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>> result = m_peerConnectionManager->m_peer_connection_factory->CreatePeerConnectionOrError(config, std::move(dependencies));
				if (result.ok()) {
					m_pc = result.MoveValue();

					webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::DataChannelInterface>>  errorOrChannel = m_pc->CreateDataChannelOrError("ServerDataChannel", NULL);
					if (errorOrChannel.ok()) {
						m_localChannel.reset(new DataChannelObserver(errorOrChannel.MoveValue()));
					} else {
						RTC_LOG(LS_ERROR) << __FUNCTION__ << "CreateDataChannel peerid:" << peerid << " error:" << errorOrChannel.error().message();
					}

				} else {
					RTC_LOG(LS_ERROR) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid << " error:" << result.error().message();
				}

				m_statsCallback = new webrtc::RefCountedObject<PeerConnectionStatsCollectorCallback>();
				RTC_LOG(LS_INFO) << __FUNCTION__ << "CreatePeerConnection peerid:" << peerid;
			};

			virtual ~PeerConnectionObserver() {
				RTC_LOG(LS_INFO) << __PRETTY_FUNCTION__;
				if (m_pc.get()) {
					// warning: pc->close call OnIceConnectionChange
					m_deleting = true;
					m_pc->Close();
				}
			}

			Json::Value getIceCandidateList() { return m_iceCandidateList; }
			
			Json::Value getStats() {
				m_statsCallback->clearReport();
				m_pc->GetStats(m_statsCallback.get());
				int count=10;
				while ( (m_statsCallback->getReport().empty()) && (--count > 0) ) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));					
				}
				return Json::Value(m_statsCallback->getReport());
			};

			webrtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };

			// PeerConnectionObserver interface
			virtual void OnAddStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface> stream)    {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " nb video tracks:" << stream->GetVideoTracks().size();
				webrtc::VideoTrackVector videoTracks = stream->GetVideoTracks();
				if (videoTracks.size()>0) {					
					m_videosink.reset(new VideoSink(videoTracks.at(0)));
				}
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " nb audio tracks:" << stream->GetAudioTracks().size();
				webrtc::AudioTrackVector audioTracks = stream->GetAudioTracks();
				if (audioTracks.size()>0) {					
					m_audiosink.reset(new AudioSink(audioTracks.at(0)));
				}
			}
			virtual void OnRemoveStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__;
				m_videosink.reset();
				m_audiosink.reset();
			}
			virtual void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__;
				m_remoteChannel.reset(new DataChannelObserver(channel));
			}
			virtual void OnRenegotiationNeeded()                              {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " peerid:" << m_peerid;;
			}

			virtual void OnIceCandidate(const webrtc::IceCandidate* candidate);
			
			virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {
				RTC_LOG(LS_WARNING) << __PRETTY_FUNCTION__ << " state:" << webrtc::PeerConnectionInterface::AsString(state) << " peerid:" << m_peerid;				
			}
			virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				RTC_LOG(LS_WARNING) << __PRETTY_FUNCTION__ << " state:" << webrtc::PeerConnectionInterface::AsString(state)  << " peerid:" << m_peerid;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{ 
					m_iceCandidateList.clear();
					if (!m_deleting) {
						std::thread([this]() {
							m_peerConnectionManager->hangUp(m_peerid);
						}).detach();
					}
				}
			}
			
			virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state) {
				RTC_LOG(LS_WARNING) << __PRETTY_FUNCTION__ << " state:" << webrtc::PeerConnectionInterface::AsString(state)  << " peerid:" << m_peerid;
				m_gatheringState = state;
			}

			uint64_t    getCreationTime() { return m_creationTime; }
			std::string getPeerId() { return m_peerid; }
			webrtc::PeerConnectionInterface::IceGatheringState getGatheringState() { return m_gatheringState; }

		private:
			PeerConnectionManager*                                   m_peerConnectionManager;
			const std::string                                        m_peerid;
			webrtc::scoped_refptr<webrtc::PeerConnectionInterface>      m_pc;
			std::unique_ptr<DataChannelObserver>                     m_localChannel;
			std::unique_ptr<DataChannelObserver>                     m_remoteChannel;
			Json::Value                                              m_iceCandidateList;
			webrtc::scoped_refptr<PeerConnectionStatsCollectorCallback> m_statsCallback;
			std::unique_ptr<VideoSink>                               m_videosink;
			std::unique_ptr<AudioSink>                               m_audiosink;
			bool                                                     m_deleting;
			uint64_t                                                 m_creationTime;
			webrtc::PeerConnectionInterface::IceGatheringState       m_gatheringState;
	};

	public:
		PeerConnectionManager(const std::list<std::string> & iceServerList, const Json::Value & config, webrtc::AudioDeviceModule::AudioLayer audioLayer, const std::string& publishFilter, const std::string& webrtcUdpPortRange, bool useNullCodec, bool usePlanB, int maxpc, webrtc::PeerConnectionInterface::IceTransportsType transportType, const std::string & basePath);
		virtual ~PeerConnectionManager();

		bool InitializePeerConnection();
		const std::map<std::string,HttpServerRequestHandler::httpFunction> getHttpApi() { return m_func; };  

		const Json::Value getIceCandidateList(const std::string &peerid);
		const Json::Value addIceCandidate(const std::string &peerid, const Json::Value& jmessage);
		const Json::Value getVideoDeviceList();
		const Json::Value getAudioDeviceList();
		const Json::Value getAudioPlayoutList();
		const Json::Value getMediaList();
		const Json::Value hangUp(const std::string &peerid);
		const Json::Value call(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value& jmessage);
		const Json::Value getIceServers(const std::string& clientIp);
		const Json::Value getPeerConnectionList();
		const Json::Value getStreamList();
		const Json::Value createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options);
		const Json::Value setAnswer(const std::string &peerid, const Json::Value& jmessage);
		std::tuple<int,std::map<std::string,std::string>,Json::Value> whep( const std::string &method,  const std::string &url,  const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value &in);


	protected:
		PeerConnectionObserver*                               CreatePeerConnection(const std::string& peerid);
		bool                                                  AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & videourl, const std::string & audiourl, const std::string & options);
		webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(const std::string & videourl, const std::map<std::string,std::string> & opts);
		webrtc::scoped_refptr<webrtc::AudioSourceInterface>      CreateAudioSource(const std::string & audiourl, const std::map<std::string,std::string> & opts);
		bool                                                  streamStillUsed(const std::string & streamLabel);
		const std::list<std::string>                          getVideoCaptureDeviceList();
		webrtc::scoped_refptr<webrtc::PeerConnectionInterface>   getPeerConnection(const std::string& peerid);
		const std::string                                     sanitizeLabel(const std::string &label);
		void                                                  createAudioModule(webrtc::AudioDeviceModule::AudioLayer audioLayer);
		std::unique_ptr<webrtc::SessionDescriptionInterface>  getAnswer(const std::string & peerid, const std::string & sdpoffer, const std::string & videourl, const std::string & audiourl, const std::string & options, bool waitgatheringcompletion = false);
		std::unique_ptr<webrtc::SessionDescriptionInterface>  getAnswer(const std::string & peerid, webrtc::SessionDescriptionInterface *session_description, const std::string & videourl, const std::string & audiourl, const std::string & options, bool waitgatheringcompletion = false);
		std::string                                           getOldestPeerCannection();


	protected:
		const webrtc::Environment                                                    m_webrtcenv;	
		std::unique_ptr<webrtc::Thread>                                              m_signalingThread;
		std::unique_ptr<webrtc::Thread>                                              m_workerThread;
		std::unique_ptr<webrtc::Thread>                                              m_networkThread;
		typedef std::pair< webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, webrtc::scoped_refptr<webrtc::AudioSourceInterface>> AudioVideoPair;
		webrtc::scoped_refptr<webrtc::AudioDecoderFactory>                           m_audioDecoderfactory;
		webrtc::scoped_refptr<webrtc::AudioDeviceModule>                             m_audioDeviceModule;
  		std::unique_ptr<webrtc::VideoDecoderFactory>                                 m_video_decoder_factory;
		webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>                m_peer_connection_factory;
		std::mutex                                                                   m_peerMapMutex;
		std::map<std::string, PeerConnectionObserver* >                              m_peer_connectionobs_map;
		std::map<std::string, AudioVideoPair>                                        m_stream_map;
		std::mutex                                                                   m_streamMapMutex;
		std::list<std::string>                                                       m_iceServerList;
		const Json::Value                                                            m_config;
		std::map<std::string,std::string>                                            m_videoaudiomap;
		const std::regex                                                             m_publishFilter;
		std::map<std::string,HttpServerRequestHandler::httpFunction>                 m_func;
		std::string																     m_webrtcPortRange;
		bool                                                                         m_useNullCodec;
		bool                                                                         m_usePlanB;
		int                                                                          m_maxpc;
		webrtc::PeerConnectionInterface::IceTransportsType                           m_transportType;
};


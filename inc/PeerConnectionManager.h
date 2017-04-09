/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.h
** 
** -------------------------------------------------------------------------*/

#ifndef PEERCONNECTIONMANAGER_H_
#define PEERCONNECTIONMANAGER_H_

#include <string>

#include "webrtc/api/peerconnectioninterface.h"

#include "webrtc/base/logging.h"
#include "webrtc/base/json.h"


class PeerConnectionManager {
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
					LOG(INFO) << __PRETTY_FUNCTION__ << " Local SDP:" << sdp;	
				}
				if (m_pc->remote_description())
				{
					m_pc->remote_description()->ToString(&sdp);				
					LOG(INFO) << __PRETTY_FUNCTION__ << " Remote SDP:" << sdp;	
				}
			}
			virtual void OnFailure(const std::string& error) 
			{
				LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
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
				LOG(INFO) << __PRETTY_FUNCTION__ << " type:" << desc->type() << " sdp:" << sdp;
				m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc), desc);
			}
			virtual void OnFailure(const std::string& error) {
				LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			CreateSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};
				
		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
		public:
			static PeerConnectionObserver* Create(PeerConnectionManager* peerConnectionManager, const std::string& peerid)
			{
				return new PeerConnectionObserver(peerConnectionManager, peerid);
			}
			void setPeerConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc) { m_pc = pc; };
			Json::Value getIceCandidateList() { return iceCandidateList_; };
			
			virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)    {}
			virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}
			virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {}
			virtual void OnRenegotiationNeeded()                              {}

			virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
			virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state)
			{
				LOG(INFO) << __PRETTY_FUNCTION__ << " " << state << " " << m_peerid;
			}
			virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				LOG(INFO) << __PRETTY_FUNCTION__ << " " << state  << " " << m_peerid;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{
					iceCandidateList_.clear();
					m_peerConnectionManager->hangUp(m_peerid);
				}
			}
			virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state)
			{
				LOG(INFO) << __PRETTY_FUNCTION__ << " " << state << " " << m_peerid;
			}

			virtual ~PeerConnectionObserver() { 
				LOG(INFO) << __PRETTY_FUNCTION__;
				m_pc->Close(); 
			}
			
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };
				
		protected:
			PeerConnectionObserver(PeerConnectionManager* peerConnectionManager, const std::string& peerid)
			: m_peerConnectionManager(peerConnectionManager)
			, m_peerid(peerid)
			, m_pc(NULL)
			{};
				
		private:
			PeerConnectionManager* m_peerConnectionManager;
			const std::string m_peerid;
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
			Json::Value iceCandidateList_;
	};

	public:
		PeerConnectionManager(const std::string & stunurl, const std::string & turnurl, const std::list<std::string> & urlList);
		virtual ~PeerConnectionManager();

		bool InitializePeerConnection();
	
		const Json::Value getIceCandidateList(const std::string &peerid);
		bool              addIceCandidate(const std::string &peerid, const Json::Value& jmessage);
		const Json::Value getDeviceList();
		bool              hangUp(const std::string &peerid);
		const Json::Value call(const std::string &peerid, const std::string &url, const std::string & options, const Json::Value& jmessage);
		const Json::Value getIceServers();
		const Json::Value getPeerConnectionList();
		const Json::Value getStreamList();
		const Json::Value createOffer(const std::string &peerid, const std::string & url, const std::string & options);
		void              setAnswer(const std::string &peerid, const Json::Value& jmessage);


	protected:
		PeerConnectionObserver* CreatePeerConnection(const std::string& peerid);
		bool                    AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & url, const std::string & options);
		cricket::VideoCapturer* OpenVideoCaptureDevice(const std::string & url, const std::string & options);
		bool                    streamStillUsed(const std::string & url);

	protected: 
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>                peer_connection_factory_;
		std::map<std::string, PeerConnectionObserver* >                           peer_connectionobs_map_;
		std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  stream_map_;
		std::string                                                               stunurl_;
		std::string                                                               turnurl_;
		std::string                                                               turnuser_;
		std::string                                                               turnpass_;
		const std::list<std::string>                                              urlList_;
};

#endif  

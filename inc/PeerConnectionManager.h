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

#include "webrtc/api/peerconnection.h"

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
			static PeerConnectionObserver* Create() { return new PeerConnectionObserver(); }
			void setPeerConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> & pc) { m_pc = pc; };
			Json::Value getIceCandidateList() { return iceCandidateList_; };
			
			virtual void OnAddStream(webrtc::MediaStreamInterface* stream)    {
				LOG(INFO) << __PRETTY_FUNCTION__;
			}
			virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) {}
			virtual void OnDataChannel(webrtc::DataChannelInterface* channel) {}
			virtual void OnRenegotiationNeeded()                              {}

			virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
                        virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {}
                        virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				LOG(INFO) << __PRETTY_FUNCTION__ << " " << state;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{
					iceCandidateList_.clear();
				}
			}
                        virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) {}
				
			virtual ~PeerConnectionObserver() { 
				LOG(INFO) << __PRETTY_FUNCTION__;
				m_pc->Close(); 
			}
			
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };
				
		protected:
			PeerConnectionObserver() : m_pc(NULL) {};
				
		private:
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
			Json::Value iceCandidateList_;
	};

	public:
		PeerConnectionManager(const std::string & stunurl);
		~PeerConnectionManager();

		bool InitializePeerConnection();
		const Json::Value getIceCandidateList(const std::string &peerid);
		void addIceCandidate(const std::string &peerid, const std::string&);
		const Json::Value getDeviceList();
		void hangUp(const std::string &peerid);
		const std::string call(std::string &peerid, const std::string & url, const std::string& message);


	protected:
		PeerConnectionObserver* CreatePeerConnection();
		bool AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & url);
		cricket::VideoCapturer* OpenVideoCaptureDevice(const std::string & url);

	protected: 
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
		std::map<std::string, PeerConnectionObserver* >  peer_connectionobs_map_;
		std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  stream_map_;
		std::string stunurl_;
};

#endif  

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** webrtc.h
** 
** -------------------------------------------------------------------------*/

#ifndef WEBRTC_H_
#define WEBRTC_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/json.h"


class DummyCreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
	public:
		static DummyCreateSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc) {  
			return  new rtc::RefCountedObject<DummyCreateSessionDescriptionObserver>(pc);  
		}
		virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
		virtual void OnFailure(const std::string& error) {
			LOG(LERROR) << __FUNCTION__ << " " << error;
		}
	protected:
		DummyCreateSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};
			
	private:
		webrtc::PeerConnectionInterface* m_pc;
};

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
	public:
		static DummySetSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc, const std::string& type) {
			return  new rtc::RefCountedObject<DummySetSessionDescriptionObserver>(pc, type);  
		}
		virtual void OnSuccess();
		virtual void OnFailure(const std::string& error) {
			LOG(LERROR) << __FUNCTION__ << " " << error;
		}
	protected:
		DummySetSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc, const std::string& type) : m_pc(pc), m_type(type) {};
			
	private:
		webrtc::PeerConnectionInterface* m_pc;
		std::string m_type;
};

class Conductor : public webrtc::PeerConnectionObserver {
	public:
		Conductor(const std::string & devid, const std::string & stunurl);
		~Conductor();

		void CreateOffer();
		const std::string getOffer();
		const Json::Value & getIceCandidateList() {return iceCandidateList_;};
		void setAnswer(const std::string&);
		void addIceCandidate(const std::string&);


	protected:
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection();
		void DeletePeerConnection();
		void AddStreams(webrtc::PeerConnectionInterface* peer_connection);
		cricket::VideoCapturer* OpenVideoCaptureDevice();

		//
		// PeerConnectionObserver implementation.
		//
		virtual void OnStateChange(webrtc::PeerConnectionObserver::StateType state_changed) {}
		virtual void OnAddStream(webrtc::MediaStreamInterface* stream) {}
		virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) {}
		virtual void OnDataChannel(webrtc::DataChannelInterface* channel) {}
		virtual void OnRenegotiationNeeded() {}
		virtual void OnIceChange() {}
		virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

	protected: 
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
		std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >  peer_connection_map_;
		std::string devid_;
		std::string stunurl_;
		Json::Value iceCandidateList_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

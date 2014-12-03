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


class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
	public:
		static SetSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc, const std::string& type) {
			return  new rtc::RefCountedObject<SetSessionDescriptionObserver>(pc, type);  
		}
		virtual void OnSuccess(){
			LOG(LERROR) << __PRETTY_FUNCTION__ << " type:" << m_type;	
		}
		virtual void OnFailure(const std::string& error) {
			LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
		}
	protected:
		SetSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc, const std::string& type) : m_pc(pc), m_type(type) {};
			
	private:
		webrtc::PeerConnectionInterface* m_pc;
		std::string m_type;
};

class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
	public:
		static CreateSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc) {  
			return  new rtc::RefCountedObject<CreateSessionDescriptionObserver>(pc);  
		}
		virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) {
			LOG(LERROR) << __PRETTY_FUNCTION__ << " type:" << desc->type();
			m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc, desc->type()), desc);
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
		static PeerConnectionObserver* Create() {
			return  new PeerConnectionObserver();  
		}
		void setPeerConnection(webrtc::PeerConnectionInterface* pc) { m_pc = pc; };
		Json::Value getIceCandidateList() { return iceCandidateList_; };
		
		virtual void OnStateChange(webrtc::PeerConnectionObserver::StateType state_changed) {}
		virtual void OnAddStream(webrtc::MediaStreamInterface* stream) {}
		virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) {}
		virtual void OnDataChannel(webrtc::DataChannelInterface* channel) {}
		virtual void OnRenegotiationNeeded() {}
		virtual void OnIceChange() {}
		virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
			
	protected:
		PeerConnectionObserver() : m_pc(NULL) {};
			
	private:
		webrtc::PeerConnectionInterface* m_pc;
		Json::Value iceCandidateList_;
};

class PeerConnectionManager {
	public:
		PeerConnectionManager(const std::string & devid, const std::string & stunurl);
		~PeerConnectionManager();

		const std::string getOffer(std::string &peerid);
		const Json::Value getIceCandidateList(const std::string &peerid);
		void setAnswer(const std::string &peerid, const std::string&);
		void addIceCandidate(const std::string &peerid, const std::string&);


	protected:
		std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, PeerConnectionObserver* > CreatePeerConnection();
		void DeletePeerConnection();
		void AddStreams(webrtc::PeerConnectionInterface* peer_connection);
		cricket::VideoCapturer* OpenVideoCaptureDevice();

	protected: 
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
		std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >  peer_connection_map_;
		std::map<std::string, PeerConnectionObserver* >  peer_connectionobs_map_;
		std::string devid_;
		std::string stunurl_;
		Json::Value iceCandidateList_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

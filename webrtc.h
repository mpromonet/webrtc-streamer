/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** webrtc.h
** 
** -------------------------------------------------------------------------*/

#ifndef PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#define PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/json.h"

class Conductor : public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver {
 public:

  Conductor(const std::string & devid);
  bool connection_active() const;
  
  void CreateOffer();
  const std::string& getOffer() {return offer_;};
  const Json::Value & getIceCandidateList() {return iceCandidateList_;};
  void setAnswer(const std::string&);


 protected:
  ~Conductor();
  bool InitializePeerConnection();
  void DeletePeerConnection();
  void AddStreams();
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

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error);

 protected: 
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  active_streams_;
  std::string devid_;
  std::string offer_;
  Json::Value iceCandidateList_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

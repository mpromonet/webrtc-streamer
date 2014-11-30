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
	 
  class VideoRenderer : public webrtc::VideoRendererInterface {
   public:
    VideoRenderer(webrtc::VideoTrackInterface* track_to_render) : width_(0), height_(0), rendered_track_(track_to_render) { rendered_track_->AddRenderer(this); }
    virtual ~VideoRenderer() { rendered_track_->RemoveRenderer(this); }

    // VideoRendererInterface implementation
    virtual void SetSize(int width, int height) { width_= width ;  height_= height; };
    virtual void RenderFrame(const cricket::VideoFrame* frame) {};

    int width() const {  return width_;  }
    int height() const {   return height_;  }

   protected: 
    int width_;
    int height_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  }; 
 
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  active_streams_;
  rtc::scoped_ptr<VideoRenderer> local_renderer_;
  std::string devid_;
  std::string offer_;
  Json::Value iceCandidateList_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#define PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/logging.h"

#include "peer_connection_client.h"

class Conductor
  : public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver,
    public PeerConnectionClientObserver {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER
  };

  Conductor(PeerConnectionClient* client, const std::string & devid);

  bool connection_active() const;

  void StartLogin(const std::string& server, int port);
  void Close();
  
  void MessageBox(const char* caption, const char* text,  bool is_error) {
	LOG(INFO) << __FUNCTION__ << " " << caption  << " " << text;
	printf("MessageBox %s %s\n", caption,text);
  }  
  void SwitchToPeerList(const Peers& peers);
  void DisconnectFromServer();
  void ConnectToPeer(int peer_id);
  void PostMessage(int msg_id, void* data);

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

  //
  // PeerConnectionClientObserver implementation.
  //
  virtual void OnSignedIn();
  virtual void OnDisconnected();
  virtual void OnPeerConnected(int id, const std::string& name);
  virtual void OnPeerDisconnected(int id);
  virtual void OnMessageFromPeer(int peer_id, const std::string& message);
  virtual void OnMessageSent(int err);
  virtual void OnServerConnectionFailure();

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
 
  // Send a message to the remote peer.
  void SendMessage(const std::string& json_object);

  int peer_id_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  PeerConnectionClient* client_;
  std::deque<std::string*> pending_messages_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  active_streams_;
  rtc::scoped_ptr<VideoRenderer> local_renderer_;
  std::string devid_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_

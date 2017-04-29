/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
** 
** -------------------------------------------------------------------------*/

#ifndef RTSPVIDEOCAPTURER_H_
#define RTSPVIDEOCAPTURER_H_

#include <string.h>
#include <vector>

#include "environment.h"
#include "rtspconnectionclient.h"

#include "webrtc/base/thread.h"

#include "webrtc/api/video_codecs/video_decoder.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/audiosource.h"
#include "webrtc/media/engine/internaldecoderfactory.h"

#include "h264_stream.h"

class RTSPVideoCapturer : public cricket::VideoCapturer, public RTSPConnection::Callback, public rtc::Thread, public webrtc::DecodedImageCallback
{
	public:
		RTSPVideoCapturer(const std::string & uri, int timeout, bool rtpovertcp);		
		virtual ~RTSPVideoCapturer();

		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp);		
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime); 
		virtual ssize_t onNewBuffer(unsigned char* buffer, ssize_t size);
                virtual void    onConnectionTimeout(RTSPConnection& connection) {
                        connection.start();
                }
                virtual void    onDataTimeout(RTSPConnection& connection)       {
                        connection.start();
                }	
		
		// overide webrtc::DecodedImageCallback
		virtual int32_t Decoded(webrtc::VideoFrame& decodedImage);

		// overide rtc::Thread
		virtual void Run();
	
		// overide cricket::VideoCapturer
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format);	  
		virtual void Stop();	  
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs);
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
		
	  
	private:
		Environment                           m_env;
		RTSPConnection                        m_connection;
		cricket::InternalDecoderFactory       m_factory;
		std::unique_ptr<webrtc::VideoDecoder> m_decoder;
		std::vector<uint8_t>                  m_cfg;	
		std::string                           m_h264_id;
		std::string                           m_pcm_id;
                h264_stream_t*                        m_h264; 
};

#endif 


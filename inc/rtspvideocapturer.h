/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <string.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "environment.h"
#include "rtspconnectionclient.h"

#include "rtc_base/thread.h"
#include "media/base/codec.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"
#include "api/video_codecs/video_decoder.h"

#include "h264_stream.h"

class Frame
{
	public:
		Frame(): m_timestamp_ms(0) {}
		Frame(std::vector<uint8_t> && content, uint64_t timestamp_ms) : m_content(content), m_timestamp_ms(timestamp_ms) {}
	
		std::vector<uint8_t> m_content;
		uint64_t m_timestamp_ms;
};

class RTSPVideoCapturer : public cricket::VideoCapturer, public RTSPConnection::Callback, public rtc::Thread, public webrtc::DecodedImageCallback
{
	public:
		RTSPVideoCapturer(const std::string & uri, int timeout, const std::string & rtptransport);
		virtual ~RTSPVideoCapturer();

		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp);
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime);
                virtual void    onConnectionTimeout(RTSPConnection& connection) {
                        connection.start();
                }
                virtual void    onDataTimeout(RTSPConnection& connection)       {
                        connection.start();
                }
                virtual void    onError(RTSPConnection& connection,const char* erro)       {
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
		
		void DecoderThread();


	private:
		Environment                           m_env;
		RTSPConnection                        m_connection;
		webrtc::InternalDecoderFactory        m_factory;
		std::unique_ptr<webrtc::VideoDecoder> m_decoder;
		std::vector<uint8_t>                  m_cfg;
		std::string                           m_codec;
                h264_stream_t*                        m_h264;
		std::queue<Frame>                     m_queue;
		std::mutex                            m_queuemutex;
		std::condition_variable               m_queuecond;
		std::thread                           m_decoderthread;
};



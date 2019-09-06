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
#include "media/base/video_common.h"
#include "media/base/video_broadcaster.h"
#include "media/engine/internal_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"

#include "decoder.h"


class RTSPVideoCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>, public RTSPConnection::Callback
{
	public:
		RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts);
		virtual ~RTSPVideoCapturer();

		static RTSPVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts) {
			return new RTSPVideoCapturer(url, opts);
		}
		
		void Start();
		void Stop();
		bool IsRunning() { return (m_stop == 0); }
		void CaptureThread() {
			m_env.mainloop();
		}

		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp) override;
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime) override;
		virtual void    onConnectionTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onDataTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onError(RTSPConnection& connection,const char* erro) override;


		// overide rtc::VideoSourceInterface<webrtc::VideoFrame>
		virtual void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override {
			m_broadcaster.AddOrUpdateSink(sink, wants);
		}

		virtual void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
			m_broadcaster.RemoveSink(sink);
		}		

	private:
		std::thread                              m_capturethread;
		char                                     m_stop;
		cricket::VideoFormat                     m_format;
		Environment                              m_env;
		RTSPConnection                           m_connection;

		std::vector<uint8_t>                     m_cfg;
		std::string                              m_codec;

		rtc::VideoBroadcaster                    m_broadcaster;
		Decoder                                  m_decoder;
};



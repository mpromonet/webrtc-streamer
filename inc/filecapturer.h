/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** filecapturer.h
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
#include "mkvclient.h"

#include "media/base/codec.h"
#include "media/base/video_common.h"
#include "media/base/video_broadcaster.h"
#include "media/engine/internal_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"



class FileVideoCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>, public MKVClient::Callback, public webrtc::DecodedImageCallback
{
	class Frame
	{
		public:
			Frame(): m_timestamp_ms(0) {}
			Frame(std::vector<uint8_t> && content, uint64_t timestamp_ms) : m_content(content), m_timestamp_ms(timestamp_ms) {}
		
			std::vector<uint8_t> m_content;
			uint64_t m_timestamp_ms;
	};

	public:
		FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts);
		virtual ~FileVideoCapturer();
	
		static FileVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts) {
			return new FileVideoCapturer(url, opts);
		}

		void Start();
		void Stop();
		bool IsRunning() { return (m_stop == 0); }
		
		void CaptureThread() {
			m_env.mainloop();
		}
		void DecoderThread();
		
		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp);
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime);

		// overide webrtc::DecodedImageCallback
		virtual int32_t Decoded(webrtc::VideoFrame& decodedImage);

		// overide rtc::VideoSourceInterface<webrtc::VideoFrame>
		void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) {
			broadcaster_.AddOrUpdateSink(sink, wants);
		}

		void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
			broadcaster_.RemoveSink(sink);
		}		

	private:
		std::thread                           m_capturethread;		
		char m_stop;
		cricket::VideoFormat                  m_format;	
		Environment                           m_env;
		MKVClient                             m_mkvclient;
		webrtc::InternalDecoderFactory        m_factory;
		std::unique_ptr<webrtc::VideoDecoder> m_decoder;
		std::vector<uint8_t>                  m_cfg;
		std::map<std::string,std::string>     m_codec;
		std::queue<Frame>                     m_queue;
		std::mutex                            m_queuemutex;
		std::condition_variable               m_queuecond;
		std::thread                           m_decoderthread;
		int                                   m_width;
		int                                   m_height;
		int                                   m_fps;
		rtc::VideoBroadcaster                 broadcaster_;
};



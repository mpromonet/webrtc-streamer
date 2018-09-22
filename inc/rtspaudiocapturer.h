/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspaudiocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <iostream>

#include "environment.h"
#include "rtspconnectionclient.h"

#include "pc/localaudiosource.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"

class RTSPAudioSource : public webrtc::Notifier<webrtc::AudioSourceInterface>, public rtc::Thread, public RTSPConnection::Callback {
	public:
		static rtc::scoped_refptr<RTSPAudioSource> Create(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts) {
			rtc::scoped_refptr<RTSPAudioSource> source(new rtc::RefCountedObject<RTSPAudioSource>(audioDecoderFactory, uri, opts));
			return source;
		}

		SourceState state() const override { return kLive; }
		bool remote() const override { return true; }
		
		void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
			RTC_LOG(INFO) << "RTSPAudioSource::AddSink ";
			m_sink = sink;
		}
		void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
			RTC_LOG(INFO) << "RTSPAudioSource::RemoveSink ";
			m_sink = NULL;
		}
		
		// overide rtc::Thread
		virtual void Run() { m_env.mainloop(); } 		

		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp);		
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime);
		
	protected:
		RTSPAudioSource(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts); 
		virtual ~RTSPAudioSource();

	private:
		Environment                             m_env;
		RTSPConnection                          m_connection; 
		rtc::scoped_refptr<webrtc::AudioDecoderFactory> m_factory;
		std::unique_ptr<webrtc::AudioDecoder>   m_decoder;
		webrtc::AudioTrackSinkInterface*        m_sink;
		int                                     m_freq;
		int                                     m_channel;
		std::queue<uint16_t>                    m_buffer;
};




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


	private:
		Environment                           m_env;
		RTSPConnection                        m_connection;
		cricket::InternalDecoderFactory       m_factory;
		std::unique_ptr<webrtc::VideoDecoder> m_decoder;
		std::vector<uint8_t>                  m_cfg;
		std::string                           m_h264_id;
                h264_stream_t*                        m_h264;
};


#include "webrtc/pc/localaudiosource.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/media/base/mediaengine.h"
#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include <iostream>

class RTSPAudioSource : public webrtc::LocalAudioSource, public rtc::Thread, public RTSPConnection::Callback {
	public:
		static rtc::scoped_refptr<RTSPAudioSource> Create(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri) {
			rtc::scoped_refptr<RTSPAudioSource> source(new rtc::RefCountedObject<RTSPAudioSource>(audioDecoderFactory, uri));
			return source;
		}

		void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
			std::cout << "=> AddSink" << std::endl;
			m_sink = sink;
		}
		void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
			std::cout << "=> RemoveSink" << std::endl;
			m_sink = NULL;
		}
		
		// overide rtc::Thread
		virtual void Run() { m_env.mainloop(); } 		

		// overide RTSPConnection::Callback
		virtual bool onNewSession(const char* id, const char* media, const char* codec, const char* sdp) {
			std::cout << "=> onNewSession " << media << "/" << codec << " sdp:" << sdp << std::endl;
			
			// parse sdp to extract freq and channel
			std::string fmt(sdp);
			size_t pos = fmt.find(codec);
			if (pos != std::string::npos) {
				fmt.erase(0, pos+strlen(codec));
				std::istringstream is (fmt);
				std::string dummy;
				std::getline(is, dummy, '/');
				std::string freq;
				std::getline(is, freq, '/');
				if (!freq.empty()) {
					m_freq = std::stoi(freq);
				}
				std::string channel;
				std::getline(is, channel, '/');
				if (!channel.empty()) {
					m_channel = std::stoi(channel);
				}
			}
			std::cout << "=> m_freq:" << m_freq << " m_channel:" << m_channel << std::endl;
			
			bool success = false;
			if ( (strcmp(media, "audio") == 0) && (strcmp(codec, "PCMU") == 0) )
			{
				m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel));
				success = true;
			}
			else if ( (strcmp(media, "audio") == 0) && (strcmp(codec, "OPUS") == 0) )
			{
				m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel));
				success = true;
			}
			else if ( (strcmp(media, "audio") == 0) && (strcmp(codec, "L16") == 0) )
			{
				m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel));
				success = true;
			}
			return success;			
		}
		
		virtual bool onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime) {
			bool success = false;
			int segmentLength = m_freq/100;
			if (m_sink) {								
				if (m_decoder.get() != NULL) {				
					int16_t decoded[size];
					webrtc::AudioDecoder::SpeechType speech_type;
					int res = m_decoder->Decode(buffer, size, m_freq, sizeof(decoded), decoded, &speech_type);
//					std::cout << "=> onData size:" << size << " decoded:" << res << std::endl;
					if (res > 0) {
						for (int i = 0 ; i < res*m_channel; ++i) {
							m_buffer.push(decoded[i]);
						}
					} else {
						std::cout << "Decode Audio failed" << std::endl;
					}																
					while (m_buffer.size() > segmentLength*m_channel) {
//						std::cout << "pop size:" << m_buffer.size() << std::endl;
						int16_t outbuffer[segmentLength*m_channel];
						for (int i=0; i<segmentLength*m_channel; ++i) {
							uint16_t value = m_buffer.front();
							outbuffer[i] = value;
							m_buffer.pop();
						}
						m_sink->OnData(outbuffer, 16, m_freq, m_channel, segmentLength);
					}
					success = true;
				} else {
					std::cout << "No Audio decoder" << std::endl;
				}
			} else {
				std::cout << "No Audio Sink" << std::endl;
			}
			return success;
		}

	protected:
		RTSPAudioSource(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri) : m_connection(m_env, this, uri.c_str()), m_factory(audioDecoderFactory), m_sink(NULL), m_freq(8000), m_channel(1) { rtc::Thread::Start(); }
		virtual ~RTSPAudioSource() override { m_env.stop(); rtc::Thread::Stop(); }


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


#endif


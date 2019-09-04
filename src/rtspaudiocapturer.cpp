/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspaudiocapturer.cpp
**
** -------------------------------------------------------------------------*/


#ifdef HAVE_LIVE555

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <cwctype>
#include <cctype>

#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

#include "rtspaudiocapturer.h"


RTSPAudioSource::RTSPAudioSource(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts) 
				: m_connection(m_env, this, uri.c_str()
					, RTSPConnection::decodeTimeoutOption(opts)
					, RTSPConnection::decodeRTPTransport(opts)
					, rtc::LogMessage::GetLogToDebug()<=2)
				, m_factory(audioDecoderFactory), m_freq(8000), m_channel(1) {
//	SetName("RTSPAudioSource", NULL);
	m_capturethread = std::thread(&RTSPAudioSource::CaptureThread, this); 
}

RTSPAudioSource::~RTSPAudioSource()  { 
	m_env.stop(); 
	m_capturethread.join();
}


// overide RTSPConnection::Callback
bool RTSPAudioSource::onNewSession(const char* id, const char* media, const char* codec, const char* sdp) {
	
	bool success = false;
	if (strcmp(media, "audio") == 0) {								
		RTC_LOG(INFO) << "RTSPAudioSource::onNewSession " << media << "/" << codec << " " << sdp;
		
		// parse sdp to extract freq and channel
		std::string fmt(sdp);
	        std::transform(fmt.begin(), fmt.end(), fmt.begin(), [](unsigned char c){ return std::tolower(c); });
		std::string codecstr(codec);
	        std::transform(codecstr.begin(), codecstr.end(), codecstr.begin(), [](unsigned char c){ return std::tolower(c); });
		size_t pos = fmt.find(codecstr);
		if (pos != std::string::npos) {
			fmt.erase(0, pos+strlen(codec));
			fmt.erase(fmt.find_first_of(" \r\n"));
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
		RTC_LOG(INFO) << "RTSPAudioSource::onNewSession code:"<< codecstr << " freq:" << m_freq << " channel:" << m_channel;
		std::map<std::string, std::string> params;
		if (m_channel == 2)
		{
			params["stereo"] = "1";
		}
		
		webrtc::SdpAudioFormat format = webrtc::SdpAudioFormat(codecstr, m_freq, m_channel, std::move(params));
		if (m_factory->IsSupportedDecoder(format)) {
			m_decoder = m_factory->MakeAudioDecoder(format,absl::optional<webrtc::AudioCodecPairId>());
			success = true;
		} else {
			RTC_LOG(LS_ERROR) << "RTSPAudioSource::onNewSession not support codec" << sdp;
		}
		
	}
	return success;			
}
		
bool RTSPAudioSource::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime) {
	bool success = false;
	int segmentLength = m_freq/100;
	if (m_decoder.get() != NULL) {
		int maxDecodedBufferSize = size*sizeof(int16_t);
		if (m_channel == 2)
		{
			maxDecodedBufferSize = (segmentLength * m_channel)*(m_channel*sizeof(int16_t));
		}
		int16_t* decoded = new int16_t[maxDecodedBufferSize];
		webrtc::AudioDecoder::SpeechType speech_type;
		int decodedBufferSize = m_decoder->Decode(buffer, size, m_freq, maxDecodedBufferSize, decoded, &speech_type);
		RTC_LOG(LS_VERBOSE) << "RTSPAudioSource::onData size:" << size << " decodedBufferSize:" << decodedBufferSize << " maxDecodedBufferSize: " << maxDecodedBufferSize << " channels: " << m_channel;
		if (decodedBufferSize > 0) {
			for (int i = 0 ; i < decodedBufferSize; ++i) {
				m_buffer.push(decoded[i]);
			}
		} else {
			RTC_LOG(LS_ERROR) << "RTSPAudioSource::onData error:Decode Audio failed";
		}	
		delete [] decoded;
		while (m_buffer.size() > segmentLength*m_channel) {
			int16_t* outbuffer = new int16_t[segmentLength*m_channel];
			for (int i=0; i<segmentLength*m_channel; ++i) {
				uint16_t value = m_buffer.front();
				outbuffer[i] = value;
				m_buffer.pop();
			}
			std::lock_guard<std::mutex> lock(m_sink_lock);
			for (auto* sink : m_sinks) {
				sink->OnData(outbuffer, 16, m_freq, m_channel, segmentLength);
			}
			delete [] outbuffer;
		}
		success = true;
	} else {
		RTC_LOG(LS_VERBOSE) << "RTSPAudioSource::onData error:No Audio decoder";
	}
	return success;
}

#endif

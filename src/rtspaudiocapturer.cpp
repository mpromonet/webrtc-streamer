/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspaudiocapturer.cpp
**
** -------------------------------------------------------------------------*/


#ifdef HAVE_LIVE555

#include "rtspaudiocapturer.h"


// overide RTSPConnection::Callback
bool RTSPAudioSource::onNewSession(const char* id, const char* media, const char* codec, const char* sdp) {
	
	bool success = false;
	if (strcmp(media, "audio") == 0) {								
		RTC_LOG(INFO) << "RTSPAudioSource::onNewSession " << media << "/" << codec << " " << sdp;
		
		// parse sdp to extract freq and channel
		std::string fmt(sdp);
		size_t pos = fmt.find(codec);
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
		RTC_LOG(INFO) << "RTSPAudioSource::onNewSession freq:" << m_freq << " channel:" << m_channel;
		
		if (strcmp(codec, "PCMU") == 0) 
		{
			m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel),absl::optional<webrtc::AudioCodecPairId>());
			success = true;
		}
		else if (strcmp(codec, "OPUS") == 0) 
		{
			m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel),absl::optional<webrtc::AudioCodecPairId>());
			success = true;
		}
		else if (strcmp(codec, "L16") == 0)
		{
			m_decoder = m_factory->MakeAudioDecoder(webrtc::SdpAudioFormat(codec, m_freq, m_channel),absl::optional<webrtc::AudioCodecPairId>());
			success = true;
		}
	}
	return success;			
}
		
bool RTSPAudioSource::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime) {
	bool success = false;
	int segmentLength = m_freq/100;
	if (m_sink) {								
		if (m_decoder.get() != NULL) {				
			int16_t decoded[size];
			webrtc::AudioDecoder::SpeechType speech_type;
			int res = m_decoder->Decode(buffer, size, m_freq, sizeof(decoded), decoded, &speech_type);
			RTC_LOG(LS_VERBOSE) << "RTSPAudioSource::onData size:" << size << " decoded:" << res;
			if (res > 0) {
				for (int i = 0 ; i < res*m_channel; ++i) {
					m_buffer.push(decoded[i]);
				}
			} else {
				RTC_LOG(LS_ERROR) << "RTSPAudioSource::onData error:Decode Audio failed";
			}																
			while (m_buffer.size() > segmentLength*m_channel) {
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
			RTC_LOG(LS_ERROR) << "RTSPAudioSource::onData error:No Audio decoder";
		}
	} else {
		RTC_LOG(LS_ERROR) << "RTSPAudioSource::onData error:No Audio Sink";
	}
	return success;
}

#endif

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspaudiocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "liveaudiosource.h"
#include "rtspconnectionclient.h"

#include "rtc_base/ref_counted_object.h"

class RTSPAudioSource : public LiveAudioSource<RTSPConnection> {
	public:
		static webrtc::scoped_refptr<RTSPAudioSource> Create(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts) {
			webrtc::scoped_refptr<RTSPAudioSource> source(new webrtc::FinalRefCountedObject<RTSPAudioSource>(audioDecoderFactory, uri, opts));
			return source;
		}
		
	protected:
		RTSPAudioSource(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts); 
		virtual ~RTSPAudioSource();

};




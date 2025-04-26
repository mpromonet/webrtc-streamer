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
#include "mkvclient.h"

#include "rtc_base/ref_counted_object.h"

class FileAudioSource : public LiveAudioSource<MKVClient> {
	public:
		static webrtc::scoped_refptr<FileAudioSource> Create(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts) {
			webrtc::scoped_refptr<FileAudioSource> source(new webrtc::FinalRefCountedObject<FileAudioSource>(audioDecoderFactory, uri, opts));
			return source;
		}
	
	protected:
		FileAudioSource(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts); 
		virtual ~FileAudioSource();
};




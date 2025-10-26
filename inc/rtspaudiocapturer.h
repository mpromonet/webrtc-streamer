/* ---------------------------------------------------------------------------
 * SPDX-License-Identifier: Unlicense
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <http://unlicense.org/>
 * -------------------------------------------------------------------------*/

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




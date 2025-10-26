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

#ifdef HAVE_LIVE555

#include "rtc_base/logging.h"

#include "rtpvideocapturer.h"

RTPVideoCapturer::RTPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) 
	: LiveVideoSource(uri, opts, videoDecoderFactory, false)
{
	RTC_LOG(LS_INFO) << "RTSPVideoCapturer " << uri ;

}

RTPVideoCapturer::~RTPVideoCapturer()
{
}


void RTPVideoCapturer::onError(SDPClient& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTPVideoCapturer:onError error:" << error;
}		


#endif

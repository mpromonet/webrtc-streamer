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

#include "filevideocapturer.h"

FileVideoCapturer::FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) 
	: LiveVideoSource(uri, opts, videoDecoderFactory, true)
{
	RTC_LOG(LS_INFO) << "FileVideoCapturer " << uri ;
}

FileVideoCapturer::~FileVideoCapturer()
{
}


#endif

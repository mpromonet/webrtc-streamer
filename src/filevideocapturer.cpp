/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** filevideocapturer.cpp
**
** -------------------------------------------------------------------------*/

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

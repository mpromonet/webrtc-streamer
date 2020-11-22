/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtpvideocapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include "rtc_base/logging.h"

#include "rtpvideocapturer.h"

RTPVideoCapturer::RTPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts) 
	: LiveVideoSource(uri, opts, false)
{
	RTC_LOG(INFO) << "RTSPVideoCapturer " << uri ;

}

RTPVideoCapturer::~RTPVideoCapturer()
{
}


void RTPVideoCapturer::onError(SDPClient& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTPVideoCapturer:onError error:" << error;
}		


#endif

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include "rtc_base/logging.h"

#include "rtspvideocapturer.h"

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) 
	: LiveVideoSource(uri, opts, videoDecoderFactory, false)
{
	RTC_LOG(LS_INFO) << "RTSPVideoCapturer " << uri ;

}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
}


void RTSPVideoCapturer::onError(RTSPConnection& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onError url:" << m_liveclient.getUrl() <<  " error:" << error;
	connection.start(1);
}		


#endif

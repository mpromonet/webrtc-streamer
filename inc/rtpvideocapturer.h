/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "sdpclient.h"

#include "livevideosource.h"


class RTPVideoCapturer : public LiveVideoSource<SDPClient>
{
	public:
		RTPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory);
		virtual ~RTPVideoCapturer();

		static RTPVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
			return new RTPVideoCapturer(url, opts, videoDecoderFactory);
		}
		
		// overide SDPClient::Callback
		virtual void    onError(SDPClient& connection,const char* error) override;
};



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



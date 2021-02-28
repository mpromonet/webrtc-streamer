/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** filecapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "mkvclient.h"

#include "livevideosource.h"

class FileVideoCapturer : public LiveVideoSource<MKVClient>
{
	public:
		FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory);
		virtual ~FileVideoCapturer();
	
		static FileVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
			return new FileVideoCapturer(url, opts, videoDecoderFactory);
		}
};



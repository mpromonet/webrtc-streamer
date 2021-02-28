/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "rtspconnectionclient.h"

#include "livevideosource.h"


class RTSPVideoCapturer : public LiveVideoSource<RTSPConnection>
{
	public:
		RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory);
		virtual ~RTSPVideoCapturer();

		static RTSPVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
			return new RTSPVideoCapturer(url, opts, videoDecoderFactory);
		}
		
		// overide RTSPConnection::Callback
		virtual void    onConnectionTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onDataTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onError(RTSPConnection& connection,const char* erro) override;
};



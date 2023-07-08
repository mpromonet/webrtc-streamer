/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VideoDecoderFactory.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "NullDecoder.h"

class VideoDecoderFactory : public webrtc::VideoDecoderFactory {
   public:
    VideoDecoderFactory(): supported_formats_({webrtc::SdpVideoFormat(cricket::kH264CodecName)}) {}
    virtual ~VideoDecoderFactory() override {}

    std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override {
   		RTC_LOG(LS_INFO) << "Create Null Decoder format:" << format.ToString();
    	return std::make_unique<NullDecoder>(format);
	}
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

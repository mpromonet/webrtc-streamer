/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VideoEncoderFactory.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "common_video/h264/h264_common.h"

#include "NullEncoder.h"

class VideoEncoderFactory : public webrtc::VideoEncoderFactory {
   public:
    VideoEncoderFactory(): supported_formats_({webrtc::SdpVideoFormat(cricket::kH264CodecName)}) {}
    virtual ~VideoEncoderFactory() override {}

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override {
      RTC_LOG(LS_INFO) << "Create Null Encoder format:" << format.ToString();
      return std::make_unique<NullEncoder>(format);
    }

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};
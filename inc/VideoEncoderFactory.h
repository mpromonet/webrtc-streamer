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

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "common_video/h264/h264_common.h"

#include "NullEncoder.h"

class VideoEncoderFactory : public webrtc::VideoEncoderFactory {
   public:
    VideoEncoderFactory(): supported_formats_(webrtc::SupportedH264Codecs()) {
      supported_formats_.push_back(webrtc::SdpVideoFormat(webrtc::kH265CodecName));
    }
    virtual ~VideoEncoderFactory() override {}

    std::unique_ptr<webrtc::VideoEncoder> Create(const webrtc::Environment & env, const webrtc::SdpVideoFormat& format) override {
      RTC_LOG(LS_INFO) << "Create Null Encoder format:" << format.ToString();
      return std::make_unique<NullEncoder>(format);
    }

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};
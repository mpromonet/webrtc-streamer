/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullDecoder.h
**
** -------------------------------------------------------------------------*/

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "EncodedVideoFrameBuffer.h"

#pragma once

class NullDecoder : public webrtc::VideoDecoder {
   public:
 	NullDecoder() {}
    virtual ~NullDecoder() override {}

	int32_t InitDecode(const webrtc::VideoCodec* codec_settings, int32_t number_of_cores) override {
		codec_settings_ = *codec_settings;
    	return WEBRTC_VIDEO_CODEC_OK;
	}
    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override {
		decoded_image_callback_ = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t Decode(const webrtc::EncodedImage& input_image, bool /*missing_frames*/, int64_t render_time_ms = -1) override {
	    if (!decoded_image_callback_) {
			RTC_LOG(LS_WARNING) << "RegisterDecodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}
		rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = input_image.GetEncodedData();
		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = new rtc::RefCountedObject<EncodedVideoFrameBuffer>(codec_settings_.width, codec_settings_.height, encodedData);
		
		webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, render_time_ms * rtc::kNumMicrosecsPerMillisec);
		frame.set_timestamp(input_image.Timestamp());
		frame.set_ntp_time_ms(input_image.NtpTimeMs());

		RTC_LOG(LS_VERBOSE) << "Decode " << frame.id() << " " << input_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY();

		decoded_image_callback_->Decoded(frame);

		return WEBRTC_VIDEO_CODEC_OK;		
	}

    const char* ImplementationName() const override { return "NullDecoder"; }

	webrtc::DecodedImageCallback* decoded_image_callback_;
	webrtc::VideoCodec codec_settings_;
};

//
// Implementation of video decoder factory
class VideoDecoderFactory : public webrtc::VideoDecoderFactory {
   public:
    VideoDecoderFactory(): supported_formats_(webrtc::SupportedH264Codecs()) {}
    virtual ~VideoDecoderFactory() override {}

    std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override {
   		RTC_LOG(INFO) << "Create Null Decoder format:" << format.name;
    	return std::make_unique<NullDecoder>();
	}
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

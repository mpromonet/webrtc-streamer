/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullEncoder.h
**
** -------------------------------------------------------------------------*/

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

#pragma once

class NullEncoder : public webrtc::VideoEncoder {
   public:
	NullEncoder() {}
    virtual ~NullEncoder() override {}

    int32_t InitEncode(const webrtc::VideoCodec* codec_settings, const webrtc::VideoEncoder::Settings& settings) override {
		RTC_LOG(LS_WARNING) << "InitEncode";
		return WEBRTC_VIDEO_CODEC_OK;
	}
    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override {
		encoded_image_callback_ = callback;
    	return WEBRTC_VIDEO_CODEC_OK;
	}
    void SetRates(const RateControlParameters& parameters) override {
		RTC_LOG(LS_WARNING) << "SetRates() " << parameters.target_bitrate.ToString() << " " << parameters.bitrate.ToString() << " " << parameters.bandwidth_allocation.kbps() << " " << parameters.framerate_fps;
	}

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frame_types) override {
	    if (!encoded_image_callback_) {
			RTC_LOG(LS_WARNING) << "RegisterEncodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = frame.video_frame_buffer();
		if (buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
			RTC_LOG(LS_WARNING) << "buffer type must be kNative";
			return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;		
		}

		webrtc::EncodedImage encoded_image;
		rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encoded_data = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer->GetI420()->DataY(), buffer->GetI420()->StrideY());
		encoded_image.SetEncodedData(encoded_data);
		encoded_image.SetTimestamp(frame.timestamp());
		encoded_image.ntp_time_ms_ = frame.ntp_time_ms();

		webrtc::CodecSpecificInfo codec_specific;
		codec_specific.codecType = webrtc::VideoCodecType::kVideoCodecH264;

		RTC_LOG(LS_VERBOSE) << "EncodedImage " << frame.id() << " " << encoded_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY() << "x" <<  buffer->GetI420()->StrideU() << "x" <<  buffer->GetI420()->StrideV();

        webrtc::EncodedImageCallback::Result result = encoded_image_callback_->OnEncodedImage(encoded_image, &codec_specific);
        if (result.error == webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED) {
            RTC_LOG(LS_ERROR) << "Error in parsing EncodedImage" << encoded_image._frameType;
		}
		return WEBRTC_VIDEO_CODEC_OK;
	}

    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override {
	    webrtc::VideoEncoder::EncoderInfo info;
		info.supports_native_handle = true;
		info.implementation_name = "NullEncoder";
		return info;
	}

  private:
	webrtc::EncodedImageCallback* encoded_image_callback_;
};


//
// Implementation of video encoder factory
class VideoEncoderFactory : public webrtc::VideoEncoderFactory {
   public:
    VideoEncoderFactory(): supported_formats_(webrtc::SupportedH264Codecs()) {}
    virtual ~VideoEncoderFactory() override {}

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override {
		RTC_LOG(INFO) << "Create Null Encoder format:" << format.name;
		return std::make_unique<NullEncoder>();
	}

    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

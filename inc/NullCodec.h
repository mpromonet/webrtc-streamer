
/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullCodec.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "rtc_base/ref_counted_object.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "absl/strings/match.h"

class EncodedVideoI420Buffer : public webrtc::I420BufferInterface {
 public:
   EncodedVideoI420Buffer(int width, int height, const webrtc::EncodedImage& encoded_image) : width_(width), height_(height), encoded_image_(encoded_image) {
  }
  virtual int width() const { return width_; }
  virtual int height() const { return height_; }
  virtual const uint8_t* DataY() const { return encoded_image_.data(); }
  virtual const uint8_t* DataU() const { return encoded_image_.data(); }
  virtual const uint8_t* DataV() const { return encoded_image_.data(); }
  virtual int StrideY() const { return encoded_image_.size(); }
  virtual int StrideU() const { return (encoded_image_.size()+1)/2; }
  virtual int StrideV() const { return (encoded_image_.size()+1)/2; }

 private:
  const int width_;
  const int height_;  
  webrtc::EncodedImage encoded_image_;
};

class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  EncodedVideoFrameBuffer(int width, int height, uint8_t* buffer, size_t length) : width_(width), height_(height), encoded_image_(buffer, length, length) {
	  buffer_ = new rtc::RefCountedObject<EncodedVideoI420Buffer>(width_, height_, encoded_image_);
  }
  virtual Type type() const { return webrtc::VideoFrameBuffer::Type::kNative; }
  virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() { return buffer_; }
  virtual int width() const { return width_; }
  virtual int height() const { return height_; }
  const webrtc::I420BufferInterface* GetI420() const final { return buffer_.get();  }

 private:
  const int width_;
  const int height_;  
  webrtc::EncodedImage encoded_image_;
  rtc::scoped_refptr<EncodedVideoI420Buffer> buffer_;
};

class NullEncoder : public webrtc::VideoEncoder {
   public:
    static std::unique_ptr<NullEncoder> Create(const cricket::VideoCodec& codec) {
		return absl::make_unique<NullEncoder>(codec);
	}
    static bool IsSupported() { return true; }
	explicit NullEncoder(const cricket::VideoCodec& codec) {
	}
    ~NullEncoder() override {}

    int32_t InitEncode(const webrtc::VideoCodec* codec_settings, const webrtc::VideoEncoder::Settings& settings) override {
		RTC_LOG(LS_WARNING) << "InitEncode()";
		return WEBRTC_VIDEO_CODEC_OK;
	}
    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override {
		encoded_image_callback_ = callback;
    	return WEBRTC_VIDEO_CODEC_OK;
	}
    void SetRates(const RateControlParameters& parameters) override {}

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frame_types) override {
	    if (!encoded_image_callback_) {
			RTC_LOG(LS_WARNING)
				<< "InitEncode() has been called, but a callback function "
				<< "has not been set with RegisterEncodeCompleteCallback()";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}
		webrtc::CodecSpecificInfo codec_specific;
		RTC_LOG(LS_ERROR) << "EncodedImage " << frame.width() << "x" << frame.height();

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = frame.video_frame_buffer();
		RTC_LOG(LS_ERROR) << "EncodedImage " <<  buffer->type() << " " <<  webrtc::VideoFrameBufferTypeToString(buffer->type());
		RTC_LOG(LS_ERROR) << "EncodedImage " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY() << "x" <<  buffer->GetI420()->StrideU() << "x" <<  buffer->GetI420()->StrideV();
		RTC_LOG(LS_ERROR) << "EncodedImage " << *(uint8_t*)buffer->GetI420()->DataY() << " " << *(uint8_t*)(buffer->GetI420()->DataY()+1) << " " << *(uint8_t*)(buffer->GetI420()->DataY()+2) << " " << *(uint8_t*)(buffer->GetI420()->DataY()+3);;
		webrtc::EncodedImage encoded_image((uint8_t*)buffer->GetI420()->DataY(), buffer->GetI420()->StrideY(), buffer->GetI420()->StrideY());
        webrtc::EncodedImageCallback::Result result = encoded_image_callback_->OnEncodedImage(encoded_image, &codec_specific);
        if (result.error == webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED) {
            RTC_LOG(LS_ERROR) << "Error in parsing EncodedImage";
        }
		return WEBRTC_VIDEO_CODEC_OK;
	}

    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override {
	    webrtc::VideoEncoder::EncoderInfo info;
		info.supports_native_handle = false;
		info.implementation_name = "RawEncoder";
		info.has_trusted_rate_controller = true;
		info.is_hardware_accelerated = true;
		info.has_internal_source = true;
		info.supports_simulcast = false;
		return info;
	}

  private:
	webrtc::EncodedImageCallback* encoded_image_callback_;
};

static bool IsSameFormat(const webrtc::SdpVideoFormat &format1,
                         const webrtc::SdpVideoFormat &format2) {
    // If different names (case insensitive), then not same formats.
    if (!absl::EqualsIgnoreCase(format1.name, format2.name)) return false;
    // For every format besides H264, comparing names is enough.
    if (!absl::EqualsIgnoreCase(format1.name.c_str(), cricket::kH264CodecName))
        return true;
    // Compare H264 profiles.
    const absl::optional<webrtc::H264::ProfileLevelId> profile_level_id = webrtc::H264::ParseSdpProfileLevelId(format1.parameters);
    const absl::optional<webrtc::H264::ProfileLevelId> other_profile_level_id = webrtc::H264::ParseSdpProfileLevelId(format2.parameters);
    // Compare H264 profiles, but not levels.
    return profile_level_id && other_profile_level_id &&
           profile_level_id->profile == other_profile_level_id->profile;
}

//
// Implementation of Raspberry video encoder factory
class VideoEncoderFactory : public webrtc::VideoEncoderFactory {
   public:
    VideoEncoderFactory() {
		RTC_LOG(LS_ERROR) << __FUNCTION__;
		supported_formats_ = webrtc::SupportedH264Codecs();
	}
    virtual ~VideoEncoderFactory() override {}

    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override {
		const cricket::VideoCodec codec(format);

		RTC_LOG(LS_ERROR) << __FUNCTION__ << format.name;

		// Try creating external encoder.
		for (const webrtc::SdpVideoFormat &supported_format : supported_formats_) {
			if (IsSameFormat(format, supported_format)) {
				RTC_LOG(INFO) << format.name << " video encoder created";
				return NullEncoder::Create(codec);
			}
		}
		return nullptr;
	}

    // Returns a list of supported codecs in order of preference.
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
        return supported_formats_;
    }

    CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override {
		for (const webrtc::SdpVideoFormat &supported_format : supported_formats_) {
			if (IsSameFormat(format, supported_format)) {
				RTC_LOG(INFO) << "Codec " << format.name << " found in RaspiVideoEncoderFactory supported format";
				// RaspiVideoEncoder only supports InternalSource H.264 Video
				// Codec
				VideoEncoderFactory::CodecInfo info;
				info.has_internal_source = true;
				return info;
			}
		}

		RTC_LOG(LS_ERROR) << "Codec " << format.name << " does not support in RaspiVideoEncoderFactory";

		VideoEncoderFactory::CodecInfo info;
		info.has_internal_source = false;
		return info;
	}

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};


class NullDecoder : public webrtc::VideoDecoder {
   public:
    static std::unique_ptr<NullDecoder> Create() {
		return std::make_unique<NullDecoder>();
	}
    static bool IsSupported() { return true; }
    ~NullDecoder() override {}

	int32_t InitDecode(const webrtc::VideoCodec* codec_settings, int32_t number_of_cores) override {
		RTC_LOG(LS_WARNING) << "InitDecode()";
		decoder_config_ = *codec_settings;
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
			RTC_LOG(LS_WARNING)
				<< "InitDecode() has been called, but a callback function "
				<< "has not been set with RegisterDecodeCompleteCallback()";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}
		rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = input_image.GetEncodedData();
		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = new rtc::RefCountedObject<EncodedVideoFrameBuffer>(encodedData->size(), 1, encodedData->data(), encodedData->size());
		RTC_LOG(LS_ERROR) << "Decode " <<  buffer->type() << " " <<  webrtc::VideoFrameBufferTypeToString(buffer->type());
		RTC_LOG(LS_ERROR) << "Decode " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY() << "x" <<  buffer->GetI420()->StrideU() << "x" <<  buffer->GetI420()->StrideV();
		RTC_LOG(LS_ERROR) << "Decode " << *(uint8_t*)buffer->GetI420()->DataY() << " " << *(uint8_t*)(buffer->GetI420()->DataY()+1) << " " << *(uint8_t*)(buffer->GetI420()->DataY()+2) << " " << *(uint8_t*)(buffer->GetI420()->DataY()+3);
		
		webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, render_time_ms * rtc::kNumMicrosecsPerMillisec);
		RTC_LOG(LS_ERROR) << "Decode " << frame.width() << "x" << frame.height();
		frame.set_timestamp(input_image.Timestamp());
		frame.set_ntp_time_ms(input_image.ntp_time_ms_);

		decoded_image_callback_->Decoded(frame);

		return WEBRTC_VIDEO_CODEC_OK;		
	}

    const char* ImplementationName() const override {
		return "NullDecoder";
	}

	webrtc::DecodedImageCallback* decoded_image_callback_;
	webrtc::VideoCodec decoder_config_;	
};

//
// Implementation of Raspberry video decoder factory
class VideoDecoderFactory : public webrtc::VideoDecoderFactory {
   public:
    VideoDecoderFactory() {
		RTC_LOG(LS_ERROR) << __FUNCTION__;
      	supported_formats_ = webrtc::SupportedH264Codecs();
	}
    virtual ~VideoDecoderFactory() override {}

    std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override {
		const cricket::VideoCodec codec(format);

   		RTC_LOG(LS_ERROR) << format.name << " video decoder created";
    	return NullDecoder::Create();
	}

    // Returns a list of supported codecs in order of preference.
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
		RTC_LOG(LS_ERROR) << __FUNCTION__;
		return supported_formats_;
	}

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

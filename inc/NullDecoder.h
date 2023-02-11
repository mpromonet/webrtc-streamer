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

	bool Configure(const webrtc::VideoDecoder::Settings& settings) override { 
		m_settings = settings;
		return true; 
	}

    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override {
		m_decoded_image_callback = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t Decode(const webrtc::EncodedImage& input_image, bool /*missing_frames*/, int64_t render_time_ms = -1) override {
	    if (!m_decoded_image_callback) {
			RTC_LOG(LS_WARNING) << "RegisterDecodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}
		rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = input_image.GetEncodedData();
		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = rtc::make_ref_counted<EncodedVideoFrameBuffer>(m_settings.max_render_resolution().Width(), m_settings.max_render_resolution().Height(), encodedData);
		
		webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, render_time_ms * rtc::kNumMicrosecsPerMillisec);
		frame.set_timestamp(input_image.Timestamp());
		frame.set_ntp_time_ms(input_image.NtpTimeMs());

		RTC_LOG(LS_VERBOSE) << "Decode " << frame.id() << " " << input_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height() << " " <<  buffer->GetI420()->StrideY();

		m_decoded_image_callback->Decoded(frame);

		return WEBRTC_VIDEO_CODEC_OK;		
	}

    const char* ImplementationName() const override { return "NullDecoder"; }

	webrtc::DecodedImageCallback* m_decoded_image_callback;
	webrtc::VideoDecoder::Settings m_settings;
};

//
// Implementation of video decoder factory
class VideoDecoderFactory : public webrtc::VideoDecoderFactory {
   public:
    VideoDecoderFactory(): supported_formats_({webrtc::SdpVideoFormat(cricket::kH264CodecName)}) {}
    virtual ~VideoDecoderFactory() override {}

    std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override {
   		RTC_LOG(LS_INFO) << "Create Null Decoder format:" << format.ToString();
    	return std::make_unique<NullDecoder>();
	}
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override { return supported_formats_; }

   private:
    std::vector<webrtc::SdpVideoFormat> supported_formats_;
};

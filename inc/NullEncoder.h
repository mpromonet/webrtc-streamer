/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** NullEncoder.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "common_video/h264/h264_common.h"
#include "modules/video_coding/include/video_codec_interface.h"

#include "EncodedVideoFrameBuffer.h"

class NullEncoder : public webrtc::VideoEncoder {
   public:
	NullEncoder(const webrtc::SdpVideoFormat& format) : m_format(format) {}
    virtual ~NullEncoder() override {}

    int32_t InitEncode(const webrtc::VideoCodec* codec_settings, const webrtc::VideoEncoder::Settings& settings) override {
		RTC_LOG(LS_ERROR) << "InitEncode format:" << m_format.name;
		return WEBRTC_VIDEO_CODEC_OK;
	}
    int32_t Release() override {
		return WEBRTC_VIDEO_CODEC_OK;
	}

    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override {
		m_encoded_image_callback = callback;
    	return WEBRTC_VIDEO_CODEC_OK;
	}
    void SetRates(const RateControlParameters& parameters) override {
		RTC_LOG(LS_VERBOSE) << "SetRates() " << parameters.target_bitrate.ToString() << " " << parameters.bitrate.ToString() << " " << parameters.bandwidth_allocation.kbps() << " " << parameters.framerate_fps;
	}

    int32_t Encode(const webrtc::VideoFrame& frame, const std::vector<webrtc::VideoFrameType>* frame_types) override {
	    if (!m_encoded_image_callback) {
			RTC_LOG(LS_ERROR) << "RegisterEncodeCompleteCallback() not called";
			return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
		}

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = frame.video_frame_buffer();
		if (buffer->type() != webrtc::VideoFrameBuffer::Type::kNative) {
			RTC_LOG(LS_ERROR) << "buffer type must be kNative";
			return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;		
		}

		EncodedVideoFrameBuffer* encodedBuffer = (EncodedVideoFrameBuffer*)buffer.get();

		// check format is consistent
		webrtc::SdpVideoFormat format = encodedBuffer->getFormat();
		if (format.name != m_format.name) {
			RTC_LOG(LS_ERROR) << "format name must be " << m_format.name << " not " << format.name;
			return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;		
		}

		// get webrtc::EncodedImage
		webrtc::EncodedImage encoded_image = encodedBuffer->getEncodedImage(frame.rtp_timestamp(), frame.ntp_time_ms());

		RTC_LOG(LS_VERBOSE) << "EncodedImage " << frame.id() << " " << encoded_image.FrameType() << " " <<  buffer->width() << "x" <<  buffer->height();

		// forward to callback
		webrtc::CodecSpecificInfo codec_specific;
		if (m_format.name == "H264") {
			codec_specific.codecType = webrtc::VideoCodecType::kVideoCodecH264;
		} 
		else if (m_format.name == "H265") {
			codec_specific.codecType = webrtc::VideoCodecType::kVideoCodecH265;
		} 
        webrtc::EncodedImageCallback::Result result = m_encoded_image_callback->OnEncodedImage(encoded_image, &codec_specific);
        if (result.error == webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED) {
            RTC_LOG(LS_ERROR) << "Error in parsing EncodedImage " << frame.id() << " " << encoded_image._frameType << " " <<  buffer->width() << "x" <<  buffer->height();
		}
		return WEBRTC_VIDEO_CODEC_OK;
	}

    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override {
	    webrtc::VideoEncoder::EncoderInfo info;
		info.supports_native_handle = true;
		info.has_trusted_rate_controller = true;
		info.implementation_name = "NullEncoder";
		return info;
	}

  private:
	webrtc::EncodedImageCallback* m_encoded_image_callback;
	webrtc::SdpVideoFormat m_format;	
};


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
 	NullDecoder(const webrtc::SdpVideoFormat& format) : m_format(format) {}
    virtual ~NullDecoder() override {}

	bool Configure(const webrtc::VideoDecoder::Settings& settings) override { 
		RTC_LOG(LS_ERROR) << "Configure format:" << m_format.name;
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
		webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = input_image.GetEncodedData();
		webrtc::scoped_refptr<webrtc::VideoFrameBuffer> frameBuffer = webrtc::make_ref_counted<EncodedVideoFrameBuffer>(m_settings.max_render_resolution().Width(), m_settings.max_render_resolution().Height(), encodedData, input_image.FrameType(), m_format);
		
		webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
					.set_video_frame_buffer(frameBuffer)
					.set_rotation(webrtc::kVideoRotation_0)
					.set_timestamp_rtp(input_image.RtpTimestamp())
					.set_timestamp_ms(render_time_ms)
					.set_ntp_time_ms(input_image.NtpTimeMs())
					.build();

		RTC_LOG(LS_VERBOSE) << "Decode " << frame.id() << " " << input_image._frameType << " " <<  frameBuffer->width() << "x" <<  frameBuffer->height();

		m_decoded_image_callback->Decoded(frame);

		return WEBRTC_VIDEO_CODEC_OK;		
	}

    const char* ImplementationName() const override { return "NullDecoder"; }

private:
	webrtc::DecodedImageCallback* m_decoded_image_callback;
	webrtc::VideoDecoder::Settings m_settings;
	webrtc::SdpVideoFormat m_format;	
};


/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** EncodedVideoFrameBuffer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "rtc_base/ref_counted_object.h"
#include "absl/strings/match.h"
#include "api/video/i420_buffer.h"

class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer
{
public:
  EncodedVideoFrameBuffer(int width, int height, const rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> &encoded_data, webrtc::VideoFrameType frameType)
    : m_width(width), m_height(height), m_encoded_data(encoded_data), m_frameType(frameType) {}
  virtual Type type() const { return webrtc::VideoFrameBuffer::Type::kNative; }
  virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() { return nullptr; }
  virtual int width() const { return m_width; }
  virtual int height() const { return m_height; }

  webrtc::EncodedImage getEncodedImage(uint32_t rtptime, int ntptime ) const { 
  	webrtc::EncodedImage encoded_image;
		encoded_image.SetEncodedData(webrtc::EncodedImageBuffer::Create(m_encoded_data->data(), m_encoded_data->size()));
		encoded_image._frameType = m_frameType;
    encoded_image.SetAtTargetQuality(true);
		encoded_image.SetRtpTimestamp(rtptime);
		encoded_image.ntp_time_ms_ = ntptime;
    return encoded_image;
  }

private:
  const int m_width;
  const int m_height;
  rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_encoded_data;
  webrtc::VideoFrameType m_frameType;
};

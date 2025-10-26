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

#include "rtc_base/ref_counted_object.h"
#include "absl/strings/match.h"
#include "api/video/i420_buffer.h"

class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer
{
public:
  EncodedVideoFrameBuffer(int width, int height, const webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> &encoded_data, webrtc::VideoFrameType frameType, const webrtc::SdpVideoFormat& format)
    : m_width(width), m_height(height), m_encoded_data(encoded_data), m_frameType(frameType), m_format(format) {}
  virtual Type type() const { return webrtc::VideoFrameBuffer::Type::kNative; }
  virtual webrtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() { return nullptr; }
  virtual int width() const { return m_width; }
  virtual int height() const { return m_height; }

  webrtc::SdpVideoFormat getFormat() const { return m_format; }

  webrtc::EncodedImage getEncodedImage(uint32_t rtptime, int ntptime ) const { 
  	webrtc::EncodedImage encoded_image;
		encoded_image.SetEncodedData(webrtc::EncodedImageBuffer::Create(m_encoded_data->data(), m_encoded_data->size()));
		encoded_image._frameType = m_frameType;
		encoded_image.SetRtpTimestamp(rtptime);
		encoded_image.ntp_time_ms_ = ntptime;
    return encoded_image;
  }

private:
  const int m_width;
  const int m_height;
  webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_encoded_data;
  webrtc::VideoFrameType m_frameType;
  webrtc::SdpVideoFormat m_format;
};

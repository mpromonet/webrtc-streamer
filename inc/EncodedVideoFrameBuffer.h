
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

class EncodedVideoI420Buffer : public webrtc::I420BufferInterface
{
public:
  EncodedVideoI420Buffer(int width, int height, const rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> &encoded_data, webrtc::VideoFrameType frameType) 
    : m_width(width), m_height(height), m_encoded_data(encoded_data), m_frameType(frameType) {}
  virtual int width() const { return m_width; }
  virtual int height() const { return m_height; }
  virtual const uint8_t *DataY() const { return m_encoded_data->data(); }
  virtual const uint8_t *DataU() const { return NULL; }
  virtual const uint8_t *DataV() const { return NULL; }
  virtual int StrideY() const { return m_encoded_data->size(); }
  virtual int StrideU() const { return 0; }
  virtual int StrideV() const { return 0; }
  webrtc::VideoFrameType getFrameType() const { return m_frameType; }

private:
  const int m_width;
  const int m_height;
  rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_encoded_data;
  webrtc::VideoFrameType m_frameType;
};

class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer
{
public:
  EncodedVideoFrameBuffer(int width, int height, const rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> &encoded_data, webrtc::VideoFrameType frameType)
    : buffer_(new rtc::RefCountedObject<EncodedVideoI420Buffer>(width, height, encoded_data, frameType)) {}
  virtual Type type() const { return webrtc::VideoFrameBuffer::Type::kNative; }
  virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() { return webrtc::I420Buffer::Create(width(), height()); }
  virtual int width() const { return buffer_->width(); }
  virtual int height() const { return buffer_->height(); }
  const webrtc::I420BufferInterface *GetI420() const final { return buffer_.get(); }

private:
  rtc::scoped_refptr<EncodedVideoI420Buffer> buffer_;
};

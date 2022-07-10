/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VideoSource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "modules/video_capture/video_capture_factory.h"
#include "media/base/video_broadcaster.h"

class VideoSource : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
  	void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) override {
		m_broadcaster.AddOrUpdateSink(sink, wants);
  	}

  	void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
		m_broadcaster.RemoveSink(sink);
  	}

protected:
	rtc::VideoBroadcaster                          m_broadcaster;
};
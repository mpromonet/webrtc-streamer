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

#include "modules/video_capture/video_capture_factory.h"
#include "media/base/video_broadcaster.h"

class VideoSource : public webrtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
  	void AddOrUpdateSink(webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const webrtc::VideoSinkWants& wants) override {
		m_broadcaster.AddOrUpdateSink(sink, wants);
  	}

  	void RemoveSink(webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
		m_broadcaster.RemoveSink(sink);
  	}

protected:
	webrtc::VideoBroadcaster                          m_broadcaster;
};

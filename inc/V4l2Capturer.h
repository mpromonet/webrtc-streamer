/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2Capturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "media/base/video_broadcaster.h"

#include "EncodedVideoFrameBuffer.h"

#include "V4l2Capture.h"

class V4l2Capturer : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
public:
	static V4l2Capturer *Create(const std::string &videourl, const std::map<std::string, std::string> &opts, std::unique_ptr<webrtc::VideoDecoderFactory> &videoDecoderFactory)
	{
		size_t width = 0;
		size_t height = 0;
		size_t fps = 0;
		if (opts.find("width") != opts.end())
		{
			width = std::stoi(opts.at("width"));
		}
		if (opts.find("height") != opts.end())
		{
			height = std::stoi(opts.at("height"));
		}
		if (opts.find("fps") != opts.end())
		{
			fps = std::stoi(opts.at("fps"));
		}

		std::unique_ptr<V4l2Capturer> capturer(new V4l2Capturer());
		if (!capturer->Init(width, height, fps, videourl))
		{
			RTC_LOG(LS_WARNING) << "Failed to create V4l2Capturer(w = " << width
								<< ", h = " << height << ", fps = " << fps
								<< ")";
			return nullptr;
		}
		return capturer.release();
	}
	virtual ~V4l2Capturer()
	{
		Destroy();
	}

private:
	V4l2Capturer() : m_stop(false) {}

	bool Init(size_t width,
			  size_t height,
			  size_t fps,
			  const std::string &videourl)
	{
		std::string device = "/dev/video0";
		if (videourl.find("v4l2://") == 0) {
			device = videourl.substr(strlen("v4l2://"));
		}		
		V4L2DeviceParameters param(device.c_str(), V4L2_PIX_FMT_H264, width, height, fps);
		m_capture.reset(V4l2Capture::create(param));

		bool ret = false;
		if (m_capture) {
			m_capturethread = std::thread(&V4l2Capturer::CaptureThread, this);
			ret = true;
		}

		return ret;
	}

	void CaptureThread()
	{
		fd_set fdset;
		FD_ZERO(&fdset);
		timeval tv;

		while (!m_stop)
		{
			tv.tv_sec=1;
			tv.tv_usec=0;	
			if (m_capture->isReadable(&tv) > 0)
			{
				char* buffer = new char[m_capture->getBufferSize()];	
				int frameSize = m_capture->read(buffer,  m_capture->getBufferSize());
				rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
				delete [] buffer;				

				int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
				rtc::scoped_refptr<webrtc::VideoFrameBuffer> frameBuffer = new rtc::RefCountedObject<EncodedVideoFrameBuffer>(m_capture->getWidth(), m_capture->getHeight(), encodedData);
				webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
					.set_video_frame_buffer(frameBuffer)
					.set_rotation(webrtc::kVideoRotation_0)
					.set_timestamp_ms(ts)
					.set_id(ts)
					.build();

				m_broadcaster.OnFrame(frame);
			}
		}
	}

	void Destroy()
	{
		if (m_capture)
		{
			m_stop = true;
			m_capturethread.join();
			m_capture.reset();
		}
	}

	void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink, const rtc::VideoSinkWants &wants) override
	{
		m_broadcaster.AddOrUpdateSink(sink, wants);
	}

	void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override
	{
		m_broadcaster.RemoveSink(sink);
	}

	bool m_stop;
	std::thread m_capturethread;
	std::unique_ptr<V4l2Capture> m_capture;
	rtc::VideoBroadcaster m_broadcaster;
};

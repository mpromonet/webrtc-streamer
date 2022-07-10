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
#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "rtc_base/logging.h"

#include "EncodedVideoFrameBuffer.h"

#include "V4l2Capture.h"
#include "VideoSource.h"

class V4l2Capturer : public VideoSource
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

    int width() { return m_width;  }
    int height() { return m_height;  }        

private:
	V4l2Capturer() : m_stop(false) {}

	bool Init(size_t width,
			  size_t height,
			  size_t fps,
			  const std::string &videourl)
	{
		m_width = width;
		m_height = height;

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

				bool idr = false;
				int cfg = 0;
				std::vector<webrtc::H264::NaluIndex> naluIndexes = webrtc::H264::FindNaluIndices((uint8_t*)buffer, frameSize);
				for (webrtc::H264::NaluIndex  index : naluIndexes) {
					webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[index.payload_start_offset]);
					RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " nalu:" << nalu_type << " payload_start_offset:" << index.payload_start_offset << " start_offset:" << index.start_offset << " size:" << index.payload_size;
					if (nalu_type ==  webrtc::H264::NaluType::kSps) {
						m_sps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						cfg++;
					}
					else if (nalu_type ==  webrtc::H264::NaluType::kPps) {
						m_pps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						cfg++;
					}
					else if (nalu_type ==  webrtc::H264::NaluType::kIdr) {
						idr = true;
					}
				}
				RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " idr:" << idr << " cfg:" << cfg << " " << m_sps->size() << " " << m_pps->size() << " " << frameSize;
				
				rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
				delete [] buffer;			
				// add last SPS/PPS if not present before an IDR
				if (idr && (cfg == 0) && (m_sps->size() != 0) && (m_pps->size() != 0) ) {
					char * newBuffer = new char[encodedData->size() + m_sps->size() + m_pps->size()];
					memcpy(newBuffer, m_sps->data(), m_sps->size());
					memcpy(newBuffer+m_sps->size(), m_pps->data(), m_pps->size());
					memcpy(newBuffer+m_sps->size()+m_pps->size(), encodedData->data(), encodedData->size());
					encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)newBuffer, encodedData->size() + m_sps->size() + m_pps->size());
					delete [] newBuffer;
				}

				int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
				rtc::scoped_refptr<webrtc::VideoFrameBuffer> frameBuffer = rtc::make_ref_counted<EncodedVideoFrameBuffer>(m_capture->getWidth(), m_capture->getHeight(), encodedData);
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

	bool                                                    m_stop;
	std::thread                                             m_capturethread;
	std::unique_ptr<V4l2Capture>                            m_capture;
	rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_sps;
	rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_pps;
	int                                                     m_width;		
 	int                                                     m_height;	
};

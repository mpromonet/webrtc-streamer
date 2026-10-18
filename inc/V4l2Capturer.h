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

#include <chrono>

#include "api/video/video_broadcaster.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_sps_parser.h"

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

    int width() const { return m_width;  }
    int height() const { return m_height;  }        

private:
	V4l2Capturer() : m_stop(false), m_width(0), m_height(0) {}

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

		V4L2DeviceParameters param(device.c_str(), std::list<uint32_t>{V4L2_PIX_FMT_HEVC, V4L2_PIX_FMT_H264}, width, height, fps);
		m_capture.reset(V4l2Capture::create(param));
		m_format = V4l2Device::fourcc(m_capture->getFormat());

		bool ret = false;
		if (m_capture) {
			m_capturethread = std::thread(&V4l2Capturer::CaptureThread, this);
			ret = true;
		}

		return ret;
	}

	void CaptureThread()
	{
		timeval tv;

		while (!m_stop)
		{
			tv.tv_sec=1;
			tv.tv_usec=0;	
			if (m_capture->isReadable(&tv) > 0)
			{
				char* buffer = new char[m_capture->getBufferSize()];	
				int frameSize = m_capture->read(buffer,  m_capture->getBufferSize());

				std::span<const uint8_t> data((const uint8_t*)buffer, frameSize);
				webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encodedData;
				webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameDelta;

				if (m_format == "H264") {
					std::vector<webrtc::H264::NaluIndex> naluIndexes = webrtc::H264::FindNaluIndices(data);
					for (webrtc::H264::NaluIndex  index : naluIndexes) {
						webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[index.payload_start_offset]);
						RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " nalu:" << nalu_type << " payload_start_offset:" << index.payload_start_offset << " start_offset:" << index.start_offset << " size:" << index.payload_size;
						if (nalu_type ==  webrtc::H264::NaluType::kSps) {
							m_sps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						}
						else if (nalu_type ==  webrtc::H264::NaluType::kPps) {
							m_pps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						}
						else if (nalu_type ==  webrtc::H264::NaluType::kIdr) {
							encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
							// add last SPS/PPS if not present before an IDR
							if ( (m_sps->size() != 0) && (m_pps->size() != 0) ) {
								char * newBuffer = new char[encodedData->size() + m_sps->size() + m_pps->size()];
								memcpy(newBuffer, m_sps->data(), m_sps->size());
								memcpy(newBuffer+m_sps->size(), m_pps->data(), m_pps->size());
								memcpy(newBuffer+m_sps->size()+m_pps->size(), encodedData->data(), encodedData->size());
								encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)newBuffer, encodedData->size() + m_sps->size() + m_pps->size());
								delete [] newBuffer;
							}
							frameType = webrtc::VideoFrameType::kVideoFrameKey;
						} else if (nalu_type ==  webrtc::H264::NaluType::kSlice) {
							encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
						}
					}
				} else if (m_format == "H265") {
					std::vector<webrtc::H265::NaluIndex> indexes = webrtc::H265::FindNaluIndices(data);
					for (webrtc::H265::NaluIndex  index : indexes) {
						webrtc::H265::NaluType nalu_type = webrtc::H265::ParseNaluType(buffer[index.payload_start_offset]);
						RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " nalu:" << nalu_type << " payload_start_offset:" << index.payload_start_offset << " start_offset:" << index.start_offset << " size:" << index.payload_size;
						if (nalu_type ==  webrtc::H265::NaluType::kVps) {
							m_vps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						}
						else if (nalu_type ==  webrtc::H265::NaluType::kSps) {
							m_sps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						}
						else if (nalu_type ==  webrtc::H265::NaluType::kPps) {
							m_pps = webrtc::EncodedImageBuffer::Create((uint8_t*)&buffer[index.start_offset], index.payload_size + index.payload_start_offset - index.start_offset);
						}
						else if ((nalu_type ==  webrtc::H265::NaluType::kIdrWRadl) || (nalu_type == webrtc::H265::NaluType::kIdrNLp)) {
							encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
							frameType = webrtc::VideoFrameType::kVideoFrameKey;
							if ( (m_vps->size() != 0) && (m_sps->size() != 0) && (m_pps->size() != 0) ) {
								char * newBuffer = new char[encodedData->size() + m_vps->size() + m_sps->size() + m_pps->size()];
								memcpy(newBuffer, m_vps->data(), m_vps->size());
								memcpy(newBuffer+m_vps->size(), m_sps->data(), m_sps->size());
								memcpy(newBuffer+m_vps->size()+m_sps->size(), m_pps->data(), m_pps->size());
								memcpy(newBuffer+m_vps->size()+m_sps->size()+m_pps->size(), encodedData->data(), encodedData->size());
								encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)newBuffer, encodedData->size() + m_vps->size() + m_sps->size() + m_pps->size());
								delete [] newBuffer;
							}
						} else if (nalu_type <=  webrtc::H265::NaluType::kRadlR) {
							encodedData = webrtc::EncodedImageBuffer::Create((uint8_t*)buffer, frameSize);
						}
					}
				}
				RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " size:" << encodedData->size() 
							<< " " << m_vps->size() 
							<< " " << m_sps->size() 
							<< " " << m_pps->size() 
							<< " " << frameSize;
				
				if (encodedData->size() != 0) {
					int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
					webrtc::scoped_refptr<webrtc::VideoFrameBuffer> frameBuffer = webrtc::make_ref_counted<EncodedVideoFrameBuffer>(m_capture->getWidth(), m_capture->getHeight(), encodedData, frameType, webrtc::SdpVideoFormat(m_format));
					webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
						.set_video_frame_buffer(frameBuffer)
						.set_rotation(webrtc::kVideoRotation_0)
						.set_timestamp_ms(ts)
						.set_id(ts)
						.build();

					m_broadcaster.OnFrame(frame);
				}
				delete [] buffer;			
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

	bool                                                       m_stop;
	std::thread                                                m_capturethread;
	std::unique_ptr<V4l2Capture>                               m_capture;
	webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_vps;
	webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_sps;
	webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> m_pps;
	std::string											       m_format;
	int                                                        m_width;		
 	int                                                        m_height;	
};

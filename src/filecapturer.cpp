/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** filecapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 
#endif

#include <chrono>
#include <iostream>

#include "rtc_base/time_utils.h"
#include "rtc_base/logging.h"


#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"

#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "filecapturer.h"
#include "Base64.hh"

uint8_t h26xmarker[] = { 0, 0, 0, 1};

FileVideoCapturer::FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts) 
	: m_env(m_stop),
	m_mkvclient(m_env, this, uri.c_str()),
	m_decoder(m_broadcaster, opts, true)
{
	RTC_LOG(INFO) << "FileVideoCapturer " << uri ;
	this->Start();	
}

FileVideoCapturer::~FileVideoCapturer()
{
	this->Stop();	
}

bool FileVideoCapturer::onNewSession(const char* id,const char* media, const char* codec, const char* sdp)
{
	if (strcmp(media, "video") == 0) {	
		RTC_LOG(INFO) << "FileVideoCapturer::onNewSession " << media << "/" << codec << " " << sdp;
		
		if (strcmp(codec, "H264") == 0)
		{
			m_codec[id] = codec;

			unsigned resultSize = 0;
			unsigned char* result = base64Decode(sdp, strlen(sdp), resultSize);

			struct timeval presentationTime;
			timerclear(&presentationTime);

			int spssize = htons(*(int16_t*)(result + 6));
			std::vector<uint8_t> sps;
			sps.insert(sps.end(), h26xmarker, h26xmarker + sizeof(h26xmarker));
			sps.insert(sps.end(), result + 8, result + 8 + spssize);
			onData(id, sps.data(), sps.size(), presentationTime);

			int ppssize = htons(*(int16_t*)(result + 9 + spssize));
			std::vector<uint8_t> pps;
			pps.insert(pps.end(), h26xmarker, h26xmarker + sizeof(h26xmarker));
			pps.insert(pps.end(), result + 11 + spssize, result + 11 + spssize + ppssize);
			onData(id, pps.data(), pps.size(), presentationTime);

			delete result;
		}
		else if (strcmp(codec, "JPEG") == 0) 
		{
			m_codec[id] = codec;
		}
		else if (strcmp(codec, "VP9") == 0) 
		{
			m_codec[id] = codec;
		}
	}
	return true; // mkv doesnot read data before all sink are started.
}

bool FileVideoCapturer::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime)
{
	int64_t ts = presentationTime.tv_sec;
	ts = ts*1000 + presentationTime.tv_usec/1000;
	RTC_LOG(LS_VERBOSE) << "FileVideoCapturer:onData id:" << id << " size:" << size << " ts:" << ts;
	int res = 0;

	std::string codec = m_codec[id];
	if (codec == "H264") {
		webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[sizeof(h26xmarker)]);	
		if (nalu_type == webrtc::H264::NaluType::kSps) {
			RTC_LOG(LS_VERBOSE) << "FileVideoCapturer:onData SPS";
			m_cfg.clear();
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);

			absl::optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(buffer+sizeof(h26xmarker)+webrtc::H264::kNaluTypeSize, size-sizeof(h26xmarker)-webrtc::H264::kNaluTypeSize);
			if (!sps) {	
				RTC_LOG(LS_ERROR) << "cannot parse sps";
				res = -1;
			}
			else
			{	
				if (m_decoder.hasDecoder()) {
					if ( (m_format.width !=  sps->width) || (m_format.height !=  sps->height) )  {
						RTC_LOG(INFO) << "format changed => set format from " << m_format.width << "x" << m_format.height	 << " to " << sps->width << "x" << sps->height;
						m_decoder.destroyDecoder();
					}
				}

				if (!m_decoder.hasDecoder()) {
					int fps = 25;
					RTC_LOG(INFO) << "FileVideoCapturer:onData SPS set format " << sps->width << "x" << sps->height << " fps:" << fps;
					cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
					m_format = videoFormat;

					m_decoder.createDecoder(codec);
				}						
			}
		}
		else if (nalu_type == webrtc::H264::NaluType::kPps) {
			RTC_LOG(LS_VERBOSE) << "FileVideoCapturer:onData PPS";
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);
		}
		else if (m_decoder.hasDecoder()) {
			webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameDelta;
			std::vector<uint8_t> content;
			if (nalu_type == webrtc::H264::NaluType::kIdr) {
				frameType = webrtc::VideoFrameType::kVideoFrameKey;
				RTC_LOG(LS_VERBOSE) << "FileVideoCapturer:onData IDR";				
				content.insert(content.end(), m_cfg.begin(), m_cfg.end());
			}
			else {
				RTC_LOG(LS_VERBOSE) << "FileVideoCapturer:onData SLICE NALU:" << nalu_type;
			}
			content.insert(content.end(), buffer, buffer+size);
			m_decoder.PostFrame(std::move(content), ts, frameType);
		} else {
			RTC_LOG(LS_ERROR) << "FileVideoCapturer:onData no decoder";
			res = -1;
		}
	} else if (codec == "JPEG") {
		int32_t width = 0;
		int32_t height = 0;
		if (libyuv::MJPGSize(buffer, size, &width, &height) == 0) {
			int stride_y = width;
			int stride_uv = (width + 1) / 2;
					
			rtc::scoped_refptr<webrtc::I420Buffer> I420buffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);
			const int conversionResult = libyuv::ConvertToI420((const uint8_t*)buffer, size,
							I420buffer->MutableDataY(), I420buffer->StrideY(),
							I420buffer->MutableDataU(), I420buffer->StrideU(),
							I420buffer->MutableDataV(), I420buffer->StrideV(),
							0, 0,
							width, height,
							width, height,
							libyuv::kRotate0, ::libyuv::FOURCC_MJPG);									
									
			if (conversionResult >= 0) {
				webrtc::VideoFrame frame(I420buffer, 0, ts, webrtc::kVideoRotation_0);
				m_decoder.Decoded(frame);
			} else {
				RTC_LOG(LS_ERROR) << "FileVideoCapturer:onData decoder error:" << conversionResult;
				res = -1;
			}
		} else {
			RTC_LOG(LS_ERROR) << "FileVideoCapturer:onData cannot JPEG dimension";
			res = -1;
		}
	} else if (codec == "VP9") {
		if (!m_decoder.hasDecoder()) {
			m_decoder.createDecoder(codec);			    
		}
		if (m_decoder.hasDecoder()) {
			webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameKey;
			std::vector<uint8_t> content;
			content.insert(content.end(), buffer, buffer+size);
			m_decoder.PostFrame(std::move(content), ts, frameType);
		}
	}

	return (res == 0);
}


void FileVideoCapturer::Start()
{
	RTC_LOG(INFO) << "FileVideoCapturer::Start";
	m_capturethread = std::thread(&FileVideoCapturer::CaptureThread, this);	
	m_decoder.Start();
}

void FileVideoCapturer::Stop()
{
	RTC_LOG(INFO) << "FileVideoCapturer::stop";
	m_env.stop();
	m_capturethread.join();
	m_decoder.Stop();
}

#endif

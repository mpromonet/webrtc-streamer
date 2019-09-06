/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 
#endif

#include <chrono>

#include "rtc_base/time_utils.h"
#include "rtc_base/logging.h"


#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"

#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "rtspvideocapturer.h"

uint8_t marker[] = { 0, 0, 0, 1};

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts) 
	: m_env(m_stop),
	m_connection(m_env, this, uri.c_str(), RTSPConnection::decodeTimeoutOption(opts), RTSPConnection::decodeRTPTransport(opts), rtc::LogMessage::GetLogToDebug()<=2),
	m_decoder(m_broadcaster, opts, false)
{
	RTC_LOG(INFO) << "RTSPVideoCapturer " << uri ;

	this->Start();
}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
	this->Stop();
}

bool RTSPVideoCapturer::onNewSession(const char* id,const char* media, const char* codec, const char* sdp)
{
	bool success = false;
	if (strcmp(media, "video") == 0) {	
		RTC_LOG(INFO) << "RTSPVideoCapturer::onNewSession " << media << "/" << codec << " " << sdp;
		
		if (strcmp(codec, "H264") == 0)
		{
			m_codec = codec;
			const char* pattern="sprop-parameter-sets=";
			const char* sprop=strstr(sdp, pattern);
			if (sprop)
			{
				std::string sdpstr(sprop+strlen(pattern));
				size_t pos = sdpstr.find_first_of(" ;\r\n");
				if (pos != std::string::npos)
				{
					sdpstr.erase(pos);
				}
				webrtc::H264SpropParameterSets sprops;
				if (sprops.DecodeSprop(sdpstr))
				{
					struct timeval presentationTime;
					timerclear(&presentationTime);

					std::vector<uint8_t> sps;
					sps.insert(sps.end(), marker, marker+sizeof(marker));
					sps.insert(sps.end(), sprops.sps_nalu().begin(), sprops.sps_nalu().end());
					onData(id, sps.data(), sps.size(), presentationTime);

					std::vector<uint8_t> pps;
					pps.insert(pps.end(), marker, marker+sizeof(marker));
					pps.insert(pps.end(), sprops.pps_nalu().begin(), sprops.pps_nalu().end());
					onData(id, pps.data(), pps.size(), presentationTime);
				}
				else
				{
					RTC_LOG(WARNING) << "Cannot decode SPS:" << sprop;
				}
			}
			success = true;
		} 
		else if (strcmp(codec, "JPEG") == 0) 
		{
			m_codec = codec;
			success = true;
		}
		else if (strcmp(codec, "VP9") == 0) 
		{
			m_codec = codec;
			success = true;
		}		
	}
	return success;
}

bool RTSPVideoCapturer::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime)
{
	int64_t ts = presentationTime.tv_sec;
	ts = ts*1000 + presentationTime.tv_usec/1000;
	RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData size:" << size << " ts:" << ts;
	int res = 0;

	if (m_codec == "H264") {
		webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[sizeof(marker)]);	
		if (nalu_type == webrtc::H264::NaluType::kSps) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS";
			m_cfg.clear();
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);

			absl::optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(buffer+sizeof(marker)+webrtc::H264::kNaluTypeSize, size-sizeof(marker)-webrtc::H264::kNaluTypeSize);
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
					RTC_LOG(INFO) << "RTSPVideoCapturer:onData SPS set format " << sps->width << "x" << sps->height << " fps:" << fps;
					cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
					m_format = videoFormat;

					m_decoder.createDecoder(m_codec);
				}
			}
		}
		else if (nalu_type == webrtc::H264::NaluType::kPps) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData PPS";
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);
		}
		else if (nalu_type == webrtc::H264::NaluType::kSei) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SEI";
			//just ignore for now
		}
		else if (m_decoder.hasDecoder()) {
			webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameDelta;
			std::vector<uint8_t> content;
			if (nalu_type == webrtc::H264::NaluType::kIdr) {
				frameType = webrtc::VideoFrameType::kVideoFrameKey;
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData IDR";				
				content.insert(content.end(), m_cfg.begin(), m_cfg.end());
			}
			else {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SLICE NALU:" << nalu_type;
			}
			content.insert(content.end(), buffer, buffer+size);
			m_decoder.PostFrame(std::move(content), ts, frameType);

		} else {
			RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData no decoder";
			res = -1;
		}
	} else if (m_codec == "JPEG") {
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
				RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData decoder error:" << conversionResult;
				res = -1;
			}
		} else {
			RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData cannot JPEG dimension";
			res = -1;
		}		
	} else if (m_codec == "VP9") {
		if (!m_decoder.hasDecoder()) {
			m_decoder.createDecoder(m_codec);			    
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

void RTSPVideoCapturer::onError(RTSPConnection& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onError url:" << m_connection.getUrl() <<  " error:" << error;
	connection.start(1);
}		


void RTSPVideoCapturer::Start()
{
	RTC_LOG(INFO) << "RTSPVideoCapturer::start";
	m_capturethread = std::thread(&RTSPVideoCapturer::CaptureThread, this);
	m_decoder.Start();
}

void RTSPVideoCapturer::Stop()
{
	RTC_LOG(INFO) << "RTSPVideoCapturer::stop";
	m_env.stop();
	m_capturethread.join();
	m_decoder.Stop();
}

#endif

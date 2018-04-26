/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include <chrono>

#include "rtc_base/timeutils.h"
#include "rtc_base/logging.h"

#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "rtspvideocapturer.h"

uint8_t marker[] = { 0, 0, 0, 1};

int decodeTimeoutOption(const std::map<std::string,std::string> & opts) {
	int timeout = 10;
	if (opts.find("timeout") != opts.end()) 
	{
		std::string timeoutString = opts.at("timeout");
		timeout = std::stoi(timeoutString);
	}
	return timeout;
}

int decodeRTPTransport(const std::map<std::string,std::string> & opts) 
{
	int rtptransport = RTSPConnection::RTPUDPUNICAST;
	if (opts.find("rtptransport") != opts.end()) 
	{
		std::string rtpTransportString = opts.at("rtptransport");
		if (rtpTransportString == "tcp") {
			rtptransport = RTSPConnection::RTPOVERTCP;
		} else if (rtpTransportString == "http") {
			rtptransport = RTSPConnection::RTPOVERHTTP;
		} else if (rtpTransportString == "multicast") {
			rtptransport = RTSPConnection::RTPUDPMULTICAST;
		}
	}
	return rtptransport;
}

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts) 
	: m_connection(m_env, this, uri.c_str(), decodeTimeoutOption(opts), decodeRTPTransport(opts), rtc::LogMessage::GetLogToDebug()<=3)
{
	RTC_LOG(INFO) << "RTSPVideoCapturer" << uri ;
	m_h264 = h264_new();
}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
	h264_free(m_h264);
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
		int nal_start = 0;
		int nal_end   = 0;
		find_nal_unit(buffer, size, &nal_start, &nal_end);
		read_nal_unit(m_h264, &buffer[nal_start], nal_end - nal_start);
		if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_SPS) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS";
			m_cfg.clear();
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);

			unsigned int width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
			unsigned int height= ((2 - m_h264->sps->frame_mbs_only_flag)* (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
			unsigned int fps=25;
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS set timing_info_present_flag:" << m_h264->sps->vui.timing_info_present_flag << " " << m_h264->sps->vui.time_scale << " " << m_h264->sps->vui.num_units_in_tick;
			if (m_decoder.get()) {
				if ( (GetCaptureFormat()->width != width) || (GetCaptureFormat()->height != height) )  {
					RTC_LOG(INFO) << "format changed => set format from " << GetCaptureFormat()->width << "x" << GetCaptureFormat()->height	 << " to " << width << "x" << height;
					m_decoder.reset(NULL);
				}
			}

			if (!m_decoder.get()) {
				RTC_LOG(INFO) << "RTSPVideoCapturer:onData SPS set format " << width << "x" << height << " fps:" << fps;
				cricket::VideoFormat videoFormat(width, height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
				SetCaptureFormat(&videoFormat);

				m_decoder=m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
				webrtc::VideoCodec codec_settings;
				codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
				m_decoder->InitDecode(&codec_settings,2);
				m_decoder->RegisterDecodeCompleteCallback(this);
			}
		}
		else if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_PPS) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData PPS";
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);
		}
		else if (m_decoder.get()) {
			std::vector<uint8_t> content;
			if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_IDR) {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData IDR";				
				content.insert(content.end(), m_cfg.begin(), m_cfg.end());
			}
			else {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SLICE NALU:" << m_h264->nal->nal_unit_type;
			}
			content.insert(content.end(), buffer, buffer+size);
			Frame frame(std::move(content), ts);			
			{
				std::unique_lock<std::mutex> lock(m_queuemutex);
				m_queue.push(frame);
			}
			m_queuecond.notify_all();
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
			const int conversionResult = libyuv::ConvertToI420((const uint8*)buffer, size,
							(uint8*)I420buffer->DataY(), I420buffer->StrideY(),
							(uint8*)I420buffer->DataU(), I420buffer->StrideU(),
							(uint8*)I420buffer->DataV(), I420buffer->StrideV(),
							0, 0,
							width, height,
							width, height,
							libyuv::kRotate0, ::libyuv::FOURCC_MJPG);									
									
			if (conversionResult >= 0) {
				webrtc::VideoFrame frame(I420buffer, 0, ts, webrtc::kVideoRotation_0);
				this->Decoded(frame);
			} else {
				RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData decoder error:" << conversionResult;
				res = -1;
			}
		} else {
			RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData cannot JPEG dimension";
			res = -1;
		}
			    
	}

	return (res == 0);
}

void RTSPVideoCapturer::DecoderThread() 
{
	while (IsRunning()) {
		std::unique_lock<std::mutex> mlock(m_queuemutex);
		while (m_queue.empty())
		{
			m_queuecond.wait(mlock);
		}
		Frame frame = m_queue.front();
		m_queue.pop();		
		mlock.unlock();
		
		RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:DecoderThread size:" << frame.m_content.size() << " ts:" << frame.m_timestamp_ms;
		uint8_t* data = frame.m_content.data();
		ssize_t size = frame.m_content.size();
		
		if (size) {
			size_t allocsize = size + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264);
			uint8_t buf[allocsize];
			memcpy( buf, data, size );

			webrtc::EncodedImage input_image(buf, size, allocsize);		
			input_image._timeStamp = frame.m_timestamp_ms; // store time in ms that overflow the 32bits
			m_decoder->Decode(input_image, false, NULL);
		}
	}
}

int32_t RTSPVideoCapturer::Decoded(webrtc::VideoFrame& decodedImage)
{
	int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;

	RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer::Decoded size:" << decodedImage.size() 
				<< " decode ts:" << decodedImage.timestamp() 
				<< " source ts:" << ts;

	// avoid to block the encoder that drop frames when sent too quickly
	static int64_t previmagets = 0;	
	static int64_t prevts = 0;
	if (prevts != 0) {
		int64_t periodSource = decodedImage.timestamp() - previmagets;
		int64_t periodDecode = ts-prevts;
			
		RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer::Decoded interframe decode:" << periodDecode << " source:" << periodSource;
		int64_t delayms = periodSource-periodDecode;
		if ( (delayms > 0) && (delayms < 1000) ) {
			usleep(delayms*1000);
		}
	}
	
	this->OnFrame(decodedImage, decodedImage.height(), decodedImage.width());
	
	previmagets = decodedImage.timestamp();
	prevts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
	
	return true;
}

cricket::CaptureState RTSPVideoCapturer::Start(const cricket::VideoFormat& format)
{
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	m_decoderthread = std::thread(&RTSPVideoCapturer::DecoderThread, this);
	return cricket::CS_RUNNING;
}

void RTSPVideoCapturer::Stop()
{
	m_env.stop();
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
	Frame frame;			
	{
		std::unique_lock<std::mutex> lock(m_queuemutex);
		m_queue.push(frame);
	}
	m_queuecond.notify_all();
	m_decoderthread.join();
}

void RTSPVideoCapturer::Run()
{
	m_env.mainloop();
}

bool RTSPVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}
#endif

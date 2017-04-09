/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.cpp
** 
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include "webrtc/base/timeutils.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"

#include "webrtc/common_video/h264/sps_parser.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/modules/video_coding/h264_sprop_parameter_sets.h"

#include "rtspvideocapturer.h"

uint8_t marker[] = { 0, 0, 0, 1};

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri) : m_connection(m_env,this,uri.c_str())
{
	LOG(INFO) << "RTSPVideoCapturer" << uri ;
}
	  
bool RTSPVideoCapturer::onNewSession(const char* id,const char* media, const char* codec, const char* sdp)
{
	LOG(INFO) << "RTSPVideoCapturer::onNewSession" << media << "/" << codec << " " << sdp;
	bool success = false;
	if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
	{
		const char* pattern="sprop-parameter-sets=";
		const char* sprop=strstr(sdp, pattern);
		if (sprop)
		{
			std::string sdpstr(sprop+strlen(pattern));
			size_t pos = sdpstr.find_first_of(" \r\n");
			if (pos != std::string::npos)
			{
				sdpstr.erase(pos);
			}
			webrtc::H264SpropParameterSets sprops;
			if (sprops.DecodeSprop(sdpstr))
			{
				std::vector<uint8_t> cfg;
				cfg.insert(cfg.end(), marker, marker+sizeof(marker));
				cfg.insert(cfg.end(), sprops.sps_nalu().begin(), sprops.sps_nalu().end());
				cfg.insert(cfg.end(), marker, marker+sizeof(marker));
				cfg.insert(cfg.end(), sprops.pps_nalu().begin(), sprops.pps_nalu().end());
				struct timeval presentationTime;
				timerclear(&presentationTime);
				onData(id, cfg.data(), cfg.size(), presentationTime);
			}
			else
			{
				LOG(WARNING) << "Cannot decode SPS:" << sprop;
			}
		}
		success = true;
	}
	return success;			
}
		
bool RTSPVideoCapturer::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime) 
{			
	LOG(INFO) << "RTSPVideoCapturer:onData size:" << size << " GetCaptureFormat:" << GetCaptureFormat();
	
	if (!m_decoder.get()) 
	{
		webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[sizeof(marker)]);
		if (nalu_type == webrtc::H264::NaluType::kSps)
		{
			rtc::Optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(buffer+sizeof(marker)+webrtc::H264::kNaluTypeSize, size-sizeof(marker)-webrtc::H264::kNaluTypeSize);
			
			if (!sps)
			{	
				LOG(LS_ERROR) << "cannot parse sps" << std::endl;
			}
			else
			{	
				LOG(INFO) << "set format " << sps->width << "x" << sps->height << std::endl;
				cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(25), cricket::FOURCC_H264);
				SetCaptureFormat(&videoFormat);
				
				m_decoder.reset(m_factory.CreateVideoDecoder(webrtc::VideoCodecType::kVideoCodecH264));
				webrtc::VideoCodec codec_settings;
				codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
				m_decoder->InitDecode(&codec_settings,2);
				m_decoder->RegisterDecodeCompleteCallback(this);						
			}
		}
	}
	if (!m_decoder) 
	{
		LOG(LS_ERROR) << "===========================onData no decoder";
		return false;
	}
	
	if (!GetCaptureFormat()) 
	{
		LOG(LS_ERROR) << "===========================onData no capture format";
		return false;
	}

	webrtc::EncodedImage input_image(buffer, size, 2*size);
	int res = m_decoder->Decode(input_image, false, NULL);
				
	return (res == 0);
}
		
ssize_t RTSPVideoCapturer::onNewBuffer(unsigned char* buffer, ssize_t size)
{
	ssize_t markerSize = 0;
	if (size > sizeof(marker))
	{
		memcpy( buffer, marker, sizeof(marker) );
		markerSize = sizeof(marker);
	}
	return 	markerSize;		
}		

int32_t RTSPVideoCapturer::Decoded(webrtc::VideoFrame& decodedImage)
{
	LOG(INFO) << "RTSPVideoCapturer::Decoded";
	
	this->OnFrame(decodedImage, decodedImage.height(), decodedImage.width());
	return true;
}

cricket::CaptureState RTSPVideoCapturer::Start(const cricket::VideoFormat& format) 
{
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	return cricket::CS_RUNNING;
}

void RTSPVideoCapturer::Stop() 
{
	m_env.stop();
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
}
		
void RTSPVideoCapturer::Run()
{	
	m_env.mainloop();
}

bool RTSPVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs) 
{
	fourccs->push_back(cricket::FOURCC_H264);
	return true;
}
#endif

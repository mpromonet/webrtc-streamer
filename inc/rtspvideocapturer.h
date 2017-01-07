/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
** 
** -------------------------------------------------------------------------*/

#ifndef RTSPVIDEOCAPTURER_H_
#define RTSPVIDEOCAPTURER_H_

#include <string.h>
#include <vector>

#include "rtspconnectionclient.h"

#include "webrtc/media/base/videocapturer.h"
#include "webrtc/base/timeutils.h"


#include "webrtc/base/optional.h"
#include "webrtc/common_video/h264/sps_parser.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/video_decoder.h"
#include "webrtc/media/engine/internaldecoderfactory.h"


uint8_t marker[] = { 0, 0, 0, 1};

class RTSPVideoCapturer : public cricket::VideoCapturer, public RTSPConnection::Callback, public rtc::Thread, public webrtc::DecodedImageCallback
{
	public:
		RTSPVideoCapturer(const std::string & uri) : m_connection(m_env,this,uri.c_str())
		{
			LOG(INFO) << "===========================RTSPVideoCapturer" << uri ;
			SetCaptureFormat(NULL);
		}
	  
		virtual ~RTSPVideoCapturer() 
		{
		}
		
		virtual bool onNewSession(const char* media, const char* codec)
		{
			LOG(INFO) << "===========================onNewSession" << media << "/" << codec;
			bool success = false;
			if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
			{
				success = true;
			}
			return success;			
		}
		
		virtual bool onData(unsigned char* buffer, ssize_t size) 
		{			
			LOG(INFO) << "===========================onData size:" << size << " GetCaptureFormat:" << GetCaptureFormat();
			
			if (!m_decoder) 
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
						
						m_decoder = m_factory.CreateVideoDecoder(webrtc::VideoCodecType::kVideoCodecH264);
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
		
		ssize_t onNewBuffer(unsigned char* buffer, ssize_t size)
		{
			ssize_t markerSize = 0;
			if (size > sizeof(marker))
			{
				memcpy( buffer, marker, sizeof(marker) );
				markerSize = sizeof(marker);
			}
			return 	markerSize;		
		}		
		
		virtual int32_t Decoded(webrtc::VideoFrame& decodedImage)
		{
			LOG(INFO) << "===========================Decoded";
			
			this->OnFrame(decodedImage, decodedImage.height(), decodedImage.width());
			return true;
		}

		virtual cricket::CaptureState Start(const cricket::VideoFormat& format) 
		{
			SetCaptureFormat(&format);
			SetCaptureState(cricket::CS_RUNNING);
			rtc::Thread::Start();
			return cricket::CS_RUNNING;
		}
	  
		virtual void Stop() 
		{
			m_env.stop();
			rtc::Thread::Stop();
			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CS_STOPPED);
		}
		
		void Run()
		{	
			m_env.mainloop();
		}
	  
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) 
		{
			fourccs->push_back(cricket::FOURCC_H264);
			return true;
		}
	  
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	  
	private:
		Environment    m_env;
		RTSPConnection m_connection;
		cricket::InternalDecoderFactory m_factory;
		webrtc::VideoDecoder* m_decoder;
};

#endif 


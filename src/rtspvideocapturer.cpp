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

#include "webrtc/modules/video_coding/h264_sprop_parameter_sets.h"

#include "rtspvideocapturer.h"

uint8_t marker[] = { 0, 0, 0, 1};

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri, int timeout, bool rtpovertcp) : m_connection(m_env, this, uri.c_str(), timeout, rtpovertcp, 1)
{
	LOG(INFO) << "RTSPVideoCapturer" << uri ;
	m_h264 = h264_new();
}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
	h264_free(m_h264);
}

bool RTSPVideoCapturer::onNewSession(const char* id,const char* media, const char* codec, const char* sdp)
{
	LOG(INFO) << "RTSPVideoCapturer::onNewSession" << media << "/" << codec << " " << sdp;
	bool success = false;
	if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
	{
		m_h264_id = id;
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

	int res = 0;

	if (m_h264_id == id) {
		int nal_start = 0;
		int nal_end   = 0;
		find_nal_unit(buffer, size, &nal_start, &nal_end);
		read_nal_unit(m_h264, &buffer[nal_start], nal_end - nal_start);
		LOG(INFO) << "NAL:" << m_h264->nal->nal_unit_type;
		if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_SPS) {
			LOG(INFO) << "===========================SPS";
			m_cfg.clear();
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);

			unsigned int width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
			unsigned int height= ((2 - m_h264->sps->frame_mbs_only_flag)* (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
			if (m_decoder.get()) {
				if ( (GetCaptureFormat()->width != width) || (GetCaptureFormat()->height != height) )  {
					LOG(INFO) << "format changed => set format from " << GetCaptureFormat()->width << "x" << GetCaptureFormat()->height	 << " to " << width << "x" << height << std::endl;
					m_decoder.reset(NULL);
				}
			}

			if (!m_decoder.get()) {
				LOG(INFO) << "set format " << width << "x" << height << std::endl;
				cricket::VideoFormat videoFormat(width, height, cricket::VideoFormat::FpsToInterval(25), cricket::FOURCC_H264);
				SetCaptureFormat(&videoFormat);

				m_decoder.reset(m_factory.CreateVideoDecoder(webrtc::VideoCodecType::kVideoCodecH264));
				webrtc::VideoCodec codec_settings;
				codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
				m_decoder->InitDecode(&codec_settings,2);
				m_decoder->RegisterDecodeCompleteCallback(this);
			}
		}
		else if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_PPS) {
			LOG(INFO) << "===========================PPS";
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);
		}
		else if (m_decoder.get()) {
			if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_IDR) {
				LOG(INFO) << "===========================IDR";
				uint8_t buf[m_cfg.size() + size];
				memcpy(buf, m_cfg.data(), m_cfg.size());
				memcpy(buf+m_cfg.size(), buffer, size);
				webrtc::EncodedImage input_image(buf, sizeof(buf), sizeof(buf) + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264));
				res = m_decoder->Decode(input_image, false, NULL);
			}
			else {
				LOG(INFO) << "===========================" << m_h264->nal->nal_unit_type;
				webrtc::EncodedImage input_image(buffer, size, size + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264));
				res = m_decoder->Decode(input_image, false, NULL);
			}
		} else {
			LOG(LS_ERROR) << "===========================onData no decoder";
			res = -1;
		}
	}

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

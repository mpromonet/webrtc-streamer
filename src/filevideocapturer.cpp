/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** FileVideoCapturer.cpp
**
** -------------------------------------------------------------------------*/

#include <chrono>

#include "rtc_base/timeutils.h"
#include "rtc_base/logging.h"


#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"

#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "rtspvideocapturer.h"
#include "FileVideoCapturer.h"


int decodeTimeoutOption(const std::map<std::string, std::string> & opts);

int decodeRTPTransport(const std::map<std::string, std::string> & opts);

FileVideoCapturer::FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts) 
{
	FILE *F = fopen(&uri[5], "rb");  // 352, height: 288,
	fseek(F, 0, SEEK_END); // seek to end of file
	FileSize = ftell(F); // get current file pointer
	fseek(F, 0, SEEK_SET);
	Buf = (uint8_t *)malloc(FileSize);
	FileSize = (int)fread(Buf, 1, FileSize, F);
	fclose(F);
	RTC_LOG(INFO) << "FileVideoCapturer" << uri ;
}

FileVideoCapturer::~FileVideoCapturer()
{
	free(Buf);
}



void FileVideoCapturer::DecoderThread() 
{
	int BufIdx=0;
	int size = 0, L;
	while (IsRunning()) {
		BufIdx += size;
		if (BufIdx >= FileSize)
			BufIdx = 0;
		for (L = BufIdx + 1; L < FileSize; L++) {
			if (Buf[L] == 0 && Buf[L + 1] == 0 && Buf[L + 2] == 0 && Buf[L + 3] == 1)
				break;
		}
		size = L - BufIdx;
		if (size > 100)
			Sleep(100);
		webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(Buf[BufIdx + 4]);
		if (nalu_type == webrtc::H264::NaluType::kSps) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS";
			m_cfg.clear();
			m_cfg.insert(m_cfg.end(), &Buf[BufIdx], &Buf[BufIdx + size]);

			absl::optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(&Buf[BufIdx + 4] + webrtc::H264::kNaluTypeSize, size - 4 - webrtc::H264::kNaluTypeSize);
			if (!sps) {
				RTC_LOG(LS_ERROR) << "cannot parse sps";
				return;
			}
			else
			{
				int fps = 25;
				if (m_decoder.get()) {
					if ((GetCaptureFormat()->width != sps->width) || (GetCaptureFormat()->height != sps->height)) {
						RTC_LOG(INFO) << "format changed => set format from " << GetCaptureFormat()->width << "x" << GetCaptureFormat()->height << " to " << sps->width << "x" << sps->height;
						m_decoder.reset(NULL);
					}
				}

				if (!m_decoder.get()) {
					RTC_LOG(INFO) << "RTSPVideoCapturer:onData SPS set format " << sps->width << "x" << sps->height << " fps:" << fps;
					cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
					SetCaptureFormat(&videoFormat);

					m_decoder = m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
					webrtc::VideoCodec codec_settings;
					codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
					m_decoder->InitDecode(&codec_settings, 2);
					m_decoder->RegisterDecodeCompleteCallback(this);
				}
			}
		}
		else if (nalu_type == webrtc::H264::NaluType::kPps) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData PPS";
			m_cfg.insert(m_cfg.end(), &Buf[BufIdx], &Buf[BufIdx+size]);
		}
		else if (m_decoder.get()) {
			int K = 0;
			if (nalu_type == webrtc::H264::NaluType::kIdr) {
				RTC_LOG(LS_VERBOSE) << "FileVideoCapturer IDR";
				memcpy(OneFrameBuf, m_cfg.data(), m_cfg.size());
				K = (int)m_cfg.size();
			}
			else {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SLICE NALU:" << nalu_type;
			}
			memcpy(&OneFrameBuf[K], &Buf[BufIdx], size);
			K += size;
			webrtc::EncodedImage input_image(OneFrameBuf, K, K + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264));
			m_decoder->Decode(input_image, false, NULL, 0);
		}
		else {
			RTC_LOG(LS_ERROR) << "FileVideoCapturer no decoder";
			break;
		}
	}
}


int32_t FileVideoCapturer::Decoded(webrtc::VideoFrame& decodedImage)
{
	this->OnFrame(decodedImage, decodedImage.height(), decodedImage.width());
	return true;
}


cricket::CaptureState FileVideoCapturer::Start(const cricket::VideoFormat& format)
{
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	m_decoderthread = std::thread(&FileVideoCapturer::DecoderThread, this);
	return cricket::CS_RUNNING;
}


void FileVideoCapturer::Stop()
{
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
	m_decoderthread.join();
}


void FileVideoCapturer::Run()
{
}


bool FileVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}

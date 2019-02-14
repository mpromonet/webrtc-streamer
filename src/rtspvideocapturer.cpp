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
	m_connection(m_env, this, uri.c_str(), RTSPConnection::decodeTimeoutOption(opts), RTSPConnection::decodeRTPTransport(opts), rtc::LogMessage::GetLogToDebug()<=3),
	m_width(0), m_height(0), m_roi_x(0), m_roi_y(0), m_roi_width(0), m_roi_height(0), m_fps(0)
{
	RTC_LOG(INFO) << "RTSPVideoCapturer " << uri ;
	if (opts.find("width") != opts.end()) {
		m_width = std::stoi(opts.at("width"));
	}
	if (opts.find("height") != opts.end()) {
		m_height = std::stoi(opts.at("height"));
	}
	if (opts.find("roi_x") != opts.end()) {
		m_roi_x = std::stoi(opts.at("roi_x"));
	if (m_roi_x<0) {
			RTC_LOG(LS_ERROR) << "Ignore roi_x=" << m_roi_x << ", it muss be >=0";
			m_roi_x = 0;
		}
	}	
	if (opts.find("roi_y") != opts.end()) {
		m_roi_y = std::stoi(opts.at("roi_y"));
		if (m_roi_y<0) {
			RTC_LOG(LS_ERROR) << "Ignore roi_<=" << m_roi_y << ", it muss be >=0";
			m_roi_y = 0;
		}
	}	
	if (opts.find("roi_width") != opts.end()) {
		m_roi_width = std::stoi(opts.at("roi_width"));
		if (m_roi_width<=0) {
			RTC_LOG(LS_ERROR) << "Ignore roi_width<=" << m_roi_width << ", it muss be >0";
			m_roi_width = 0;
		}
	}	
	if (opts.find("roi_height") != opts.end()) {
		m_roi_height = std::stoi(opts.at("roi_height"));
		if (m_roi_height<=0) {
			RTC_LOG(LS_ERROR) << "Ignore roi_height<=" << m_roi_height << ", it muss be >0";
			m_roi_height = 0;
		}
	}
	if (opts.find("fps") != opts.end()) {
		m_fps = std::stoi(opts.at("fps"));
	}
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
				if (m_decoder.get()) {
					if ( (m_format.width !=  sps->width) || (m_format.height !=  sps->height) )  {
						RTC_LOG(INFO) << "format changed => set format from " << m_format.width << "x" << m_format.height	 << " to " << sps->width << "x" << sps->height;
						m_decoder.reset(NULL);
					}
				}

				if (!m_decoder.get()) {
					int fps = 25;
					RTC_LOG(INFO) << "RTSPVideoCapturer:onData SPS set format " << sps->width << "x" << sps->height << " fps:" << fps;
					cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
					m_format = videoFormat;

					m_decoder=m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
					webrtc::VideoCodec codec_settings;
					codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
					m_decoder->InitDecode(&codec_settings,2);
					m_decoder->RegisterDecodeCompleteCallback(this);
				}
			}
		}
		else if (nalu_type == webrtc::H264::NaluType::kPps) {
			RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData PPS";
			m_cfg.insert(m_cfg.end(), buffer, buffer+size);
		}
		else if (m_decoder.get()) {
			std::vector<uint8_t> content;
			if (nalu_type == webrtc::H264::NaluType::kIdr) {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData IDR";				
				content.insert(content.end(), m_cfg.begin(), m_cfg.end());
			}
			else {
				RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SLICE NALU:" << nalu_type;
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

void RTSPVideoCapturer::onError(RTSPConnection& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onError url:" << m_connection.getUrl() <<  " error:" << error;
	connection.start(1);
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
			uint8_t* buf = new uint8_t[allocsize];
			memcpy( buf, data, size );

			webrtc::EncodedImage input_image(buf, size, allocsize);		
			input_image.SetTimestamp(frame.m_timestamp_ms); // store time in ms that overflow the 32bits
			m_decoder->Decode(input_image, false, NULL,0);
			delete [] buf;
		}
	}
}

int32_t RTSPVideoCapturer::Decoded(webrtc::VideoFrame& decodedImage)
{
	int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;

	RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer::Decoded size:" << decodedImage.size() 
				<< " decode ts:" << decodedImage.timestamp() 
				<< " source ts:" << ts;

	// restore the timestamp that had overflow in the convertion EncodedImage.SetTimeStamp(presentationTime)
	// rdp timestamp is just 32 bit, overflow error will occur each ~50 days
	decodedImage.set_timestamp_us((int64_t)decodedImage.timestamp() * 1000);

	if(m_roi_x >= decodedImage.width()) {
		RTC_LOG(LS_ERROR) << "The ROI position protrudes beyond the right edge of the image. Ignore roi_x.";
		m_roi_x = 0;
	}
	if(m_roi_y >= decodedImage.height()) {
		RTC_LOG(LS_ERROR) << "The ROI position protrudes beyond the bottom edge of the image. Ignore roi_y.";
		m_roi_y = 0;
	}
	if(m_roi_width != 0 && (m_roi_width + m_roi_x) > decodedImage.width()) {
		RTC_LOG(LS_ERROR) << "The ROI protrudes beyond the right edge of the image. Ignore roi_width.";
		m_roi_width = 0;
	}
	if(m_roi_height != 0 && (m_roi_height + m_roi_y) > decodedImage.height()) {
		RTC_LOG(LS_ERROR) << "The ROI protrudes beyond the bottom edge of the image. Ignore roi_height.";
		m_roi_height = 0;
	}
	
	if(m_roi_width == 0) {
		m_roi_width = decodedImage.width() - m_roi_x;
	}
	if(m_roi_height == 0) {
		m_roi_height = decodedImage.height() - m_roi_y;
	}

	// source image is croped but destination image size is not set
	if((m_roi_width != decodedImage.width() || m_roi_height != decodedImage.height()) && (m_height == 0 && m_width == 0)) {
		m_height = m_roi_height;
		m_width = m_roi_width;
	}

	if ( (m_height == 0) && (m_width == 0) ) {
		broadcaster_.OnFrame(decodedImage);
	} else {
		int height = m_height;
		int width = m_width;
		if (height == 0) {
			height = (m_roi_height * width) / m_roi_width;
		}
		else if (width == 0) {
			width = (m_roi_width * height) / m_roi_height;
		}
		int stride_y = width;
		int stride_uv = (width + 1) / 2;
		rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);
		if(m_roi_width != decodedImage.width() || m_roi_height != decodedImage.height()) {
			scaled_buffer->CropAndScaleFrom(*decodedImage.video_frame_buffer()->ToI420(), m_roi_x, m_roi_y, m_roi_width, m_roi_height);
		} else {
			scaled_buffer->ScaleFrom(*decodedImage.video_frame_buffer()->ToI420());
		}
		webrtc::VideoFrame frame = webrtc::VideoFrame(scaled_buffer, decodedImage.timestamp(),
			decodedImage.render_time_ms(), webrtc::kVideoRotation_0);
				
		broadcaster_.OnFrame(frame);
	}
		
	return true;
}

void RTSPVideoCapturer::Start()
{
	RTC_LOG(INFO) << "RTSPVideoCapturer::start";
	SetName("RTSPVideoCapturer", NULL);
	rtc::Thread::Start();
	m_decoderthread = std::thread(&RTSPVideoCapturer::DecoderThread, this);
}

void RTSPVideoCapturer::Stop()
{
	RTC_LOG(INFO) << "RTSPVideoCapturer::stop";
	m_env.stop();
	rtc::Thread::Stop();
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

#endif

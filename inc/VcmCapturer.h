/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VcmCapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "modules/video_capture/video_capture_factory.h"
#include "VideoSource.h"

class VcmCapturer : public rtc::VideoSinkInterface<webrtc::VideoFrame>,  public VideoSource {
 public:
	static VcmCapturer* Create(const std::string & videourl, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
		std::unique_ptr<VcmCapturer> vcm_capturer(new VcmCapturer());
		size_t width = 0;
		size_t height = 0;
		size_t fps = 0;
		if (opts.find("width") != opts.end()) {
			width = std::stoi(opts.at("width"));
		}
		if (opts.find("height") != opts.end()) {
			height = std::stoi(opts.at("height"));
		}
		if (opts.find("fps") != opts.end()) {
			fps = std::stoi(opts.at("fps"));
		}	
		if (!vcm_capturer->Init(width, height, fps, videourl)) {
			RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << width
								<< ", h = " << height << ", fps = " << fps
								<< ")";
			return nullptr;
		}
		return vcm_capturer.release();
	}
	virtual ~VcmCapturer() {
		Destroy();
	}
	int width() { return m_width;  }
	int height() { return m_height;  }

  void OnFrame(const webrtc::VideoFrame& frame) override {
	  m_broadcaster.OnFrame(frame);
  } 

 private:
  VcmCapturer() : m_vcm(nullptr) {}
  
  bool Init(size_t width,
            size_t height,
            size_t target_fps,
            const std::string & videourl) {
	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
	m_width = width;
	m_height = height;

	std::string deviceId;
	int num_videoDevices = device_info->NumberOfDevices();
	RTC_LOG(LS_INFO) << "nb video devices:" << num_videoDevices;
	const uint32_t kSize = 256;
	char name[kSize] = {0};
	char id[kSize] = {0};
	if (videourl.find("videocap://") == 0) {
		int deviceNumber = atoi(videourl.substr(strlen("videocap://")).c_str());
		RTC_LOG(LS_WARNING) << "device number:" << deviceNumber;
		if (device_info->GetDeviceName(deviceNumber, name, kSize, id, kSize) == 0)
		{
			deviceId = id;
		}

	} else {
		for (int i = 0; i < num_videoDevices; ++i) {
			if (device_info->GetDeviceName(i, name, kSize, id, kSize) == 0)
			{
				if (videourl == name) {
					deviceId = id;
					break;
				}
			}
		}
	}

	if (deviceId.empty()) {
		RTC_LOG(LS_WARNING) << "device not found:" << videourl;
		Destroy();
		return false;	
	}

	m_vcm = webrtc::VideoCaptureFactory::Create(deviceId.c_str());
	m_vcm->RegisterCaptureDataCallback(this);

	webrtc::VideoCaptureCapability capability;
	capability.width = static_cast<int32_t>(width);
	capability.height = static_cast<int32_t>(height);
	capability.maxFPS = static_cast<int32_t>(target_fps);
	capability.videoType = webrtc::VideoType::kI420;

	if (device_info->GetBestMatchedCapability(m_vcm->CurrentDeviceName(), capability, capability)<0) {
		device_info->GetCapability(m_vcm->CurrentDeviceName(), 0, capability);
	}

	if (m_vcm->StartCapture(capability) != 0) {
		Destroy();
		return false;
	}

	RTC_CHECK(m_vcm->CaptureStarted());

	return true;
  }

  void Destroy() {
	if (m_vcm) {
		m_vcm->StopCapture();
		m_vcm->DeRegisterCaptureDataCallback();
		m_vcm = nullptr;
	} 
  }

	int                                            m_width;		
 	int                                            m_height;
	rtc::scoped_refptr<webrtc::VideoCaptureModule> m_vcm;
};

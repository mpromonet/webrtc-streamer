/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** capturerfactory.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <regex>

#include "modules/video_capture/video_capture_factory.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#include "filecapturer.h"
#endif

#ifdef USE_X11
#include "screencapturer.h"
#endif

#include "pc/video_track_source.h"

class VcmCapturer : public rtc::VideoSinkInterface<webrtc::VideoFrame>,  public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  static VcmCapturer* Create(const std::string & videourl, const std::map<std::string, std::string> & opts) {
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

  void OnFrame(const webrtc::VideoFrame& frame) override {
	  broadcaster_.OnFrame(frame);
  } 

 private:
  VcmCapturer() : vcm_(nullptr) {}
  
  bool Init(size_t width,
            size_t height,
            size_t target_fps,
            const std::string & videourl) {
	std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(webrtc::VideoCaptureFactory::CreateDeviceInfo());

	int num_videoDevices = device_info->NumberOfDevices();
	RTC_LOG(INFO) << "nb video devices:" << num_videoDevices;
	std::string deviceId;
	for (int i = 0; i < num_videoDevices; ++i) {
		const uint32_t kSize = 256;
		char name[kSize] = {0};
		char id[kSize] = {0};
		if (device_info->GetDeviceName(i, name, kSize, id, kSize) == 0)
		{
			if (videourl == name) {
				deviceId = id;
				break;
			}
		}
	}

	if (deviceId.empty()) {
		RTC_LOG(LS_WARNING) << "device not found:" << videourl;
		Destroy();
		return false;	
	}

	vcm_ = webrtc::VideoCaptureFactory::Create(deviceId.c_str());
	vcm_->RegisterCaptureDataCallback(this);

	webrtc::VideoCaptureCapability capability;
	capability.width = static_cast<int32_t>(width);
	capability.height = static_cast<int32_t>(height);
	capability.maxFPS = static_cast<int32_t>(target_fps);
	capability.videoType = webrtc::VideoType::kI420;

	if (device_info->GetBestMatchedCapability(vcm_->CurrentDeviceName(), capability, capability)<0) {
		device_info->GetCapability(vcm_->CurrentDeviceName(), 0, capability);
	}

	if (vcm_->StartCapture(capability) != 0) {
		Destroy();
		return false;
	}

	RTC_CHECK(vcm_->CaptureStarted());

	return true;
  }

  void Destroy() {
	if (vcm_) {
		vcm_->StopCapture();
		vcm_->DeRegisterCaptureDataCallback();
		vcm_ = nullptr;
	} 
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& wants) {
	broadcaster_.AddOrUpdateSink(sink, wants);
  }

  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
	broadcaster_.RemoveSink(sink);
  }

  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
  rtc::VideoBroadcaster broadcaster_;
};

template<class T>
class TrackSource : public webrtc::VideoTrackSource {
public:
	static rtc::scoped_refptr<TrackSource> Create(const std::string & videourl, const std::map<std::string, std::string> & opts) {
		std::unique_ptr<T> capturer = absl::WrapUnique(T::Create(videourl, opts));
		if (!capturer) {
			return nullptr;
		}
		return new rtc::RefCountedObject<TrackSource>(std::move(capturer));
	}

protected:
	explicit TrackSource(std::unique_ptr<T> capturer)
		: webrtc::VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

private:
	rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
		return capturer_.get();
	}
	std::unique_ptr<T> capturer_;
};

class CapturerFactory {
	public:

	static const std::list<std::string> GetVideoCaptureDeviceList(const std::regex & publishFilter)
	{
		std::list<std::string> videoDeviceList;

		if (std::regex_match("videocap://",publishFilter)) {
			std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
			if (info)
			{
				int num_videoDevices = info->NumberOfDevices();
				RTC_LOG(INFO) << "nb video devices:" << num_videoDevices;
				for (int i = 0; i < num_videoDevices; ++i)
				{
					const uint32_t kSize = 256;
					char name[kSize] = {0};
					char id[kSize] = {0};
					if (info->GetDeviceName(i, name, kSize, id, kSize) != -1)
					{
						RTC_LOG(INFO) << "video device name:" << name << " id:" << id;
						videoDeviceList.push_back(name);
					}
				}
			}
		}

		return videoDeviceList;
	}
	
	static const std::list<std::string> GetVideoSourceList(const std::regex & publishFilter) {
	
		std::list<std::string> videoList;
		
#ifdef USE_X11
		if (std::regex_match("window://",publishFilter)) {
			std::unique_ptr<webrtc::DesktopCapturer> capturer = webrtc::DesktopCapturer::CreateWindowCapturer(webrtc::DesktopCaptureOptions::CreateDefault());	
			if (capturer) {
				webrtc::DesktopCapturer::SourceList sourceList;
				if (capturer->GetSourceList(&sourceList)) {
					for (auto source : sourceList) {
						std::ostringstream os;
						os << "window://" << source.title;
						videoList.push_back(os.str());
					}
				}
			}
		}
		if (std::regex_match("screen://",publishFilter)) {
			std::unique_ptr<webrtc::DesktopCapturer> capturer = webrtc::DesktopCapturer::CreateScreenCapturer(webrtc::DesktopCaptureOptions::CreateDefault());		
			if (capturer) {
				webrtc::DesktopCapturer::SourceList sourceList;
				if (capturer->GetSourceList(&sourceList)) {
					for (auto source : sourceList) {
						std::ostringstream os;
						os << "screen://" << source.id;
						videoList.push_back(os.str());
					}
				}
			}
		}
#endif		
		return videoList;
	}

	static rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(const std::string & videourl, const std::map<std::string,std::string> & opts, const std::regex & publishFilter, rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory, const webrtc::FakeConstraints & constraints) {
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource;
		if ((videourl.find("rtsp://") == 0) && (std::regex_match("rtsp://", publishFilter))) {
#ifdef HAVE_LIVE555
			videoSource = TrackSource<RTSPVideoCapturer>::Create(videourl,opts);
#endif	
		}
		else if ((videourl.find("file://") == 0) && (std::regex_match("file://", publishFilter)))
		{
#ifdef HAVE_LIVE555
			videoSource = TrackSource<FileVideoCapturer>::Create(videourl, opts);
#endif
		}
		else if ((videourl.find("screen://") == 0) && (std::regex_match("screen://", publishFilter)))
		{
#ifdef USE_X11
			videoSource = TrackSource<ScreenCapturer>::Create(videourl, opts);
#endif	
		}
		else if ((videourl.find("window://") == 0) && (std::regex_match("window://", publishFilter)))
		{
#ifdef USE_X11
			videoSource = TrackSource<WindowCapturer>::Create(videourl, opts);
#endif	
		}
		else if (std::regex_match("videocap://",publishFilter)) {
			videoSource = TrackSource<VcmCapturer>::Create(videourl, opts);
		}
		return videoSource;
	}

};

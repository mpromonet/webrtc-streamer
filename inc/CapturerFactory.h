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
#include "media/engine/webrtcvideocapturerfactory.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#include "rtspaudiocapturer.h"
#endif
#include "FileVideoCapturer.h"

#ifdef USE_X11
#include "screencapturer.h"
#endif

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

	
	static std::unique_ptr<cricket::VideoCapturer> CreateVideoCapturer(const std::string & videourl, const std::map<std::string,std::string> & opts, const std::regex & publishFilter) {
		std::unique_ptr<cricket::VideoCapturer> capturer;
		if ((videourl.find("file:") == 0) && (std::regex_match("file:", publishFilter)))
		{
			capturer.reset(new FileVideoCapturer(videourl, opts));
		}
		else if ((videourl.find("rtsp://") == 0) && (std::regex_match("rtsp://", publishFilter)))
		{
#ifdef HAVE_LIVE555
			capturer.reset(new RTSPVideoCapturer(videourl, opts));
#endif
		}
		else if ( (videourl.find("screen://") == 0) && (std::regex_match("screen://",publishFilter)) )
		{
#ifdef USE_X11
			capturer.reset(new ScreenCapturer(videourl, opts));			
#endif	
		}
		else if ( (videourl.find("window://") == 0) && (std::regex_match("window://",publishFilter)) )
		{
#ifdef USE_X11
			capturer.reset(new WindowCapturer(videourl, opts));			
#endif	
		}
		else if (std::regex_match("videocap://",publishFilter)) {		
			cricket::WebRtcVideoDeviceCapturerFactory factory;
			capturer = factory.Create(cricket::Device(videourl, 0));
		}
		
		return capturer;
	}	
};

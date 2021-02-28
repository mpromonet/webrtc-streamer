/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** CapturerFactory.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <regex>

#include "VcmCapturer.h"

#ifdef HAVE_LIVE555
#include "rtspvideocapturer.h"
#include "rtpvideocapturer.h"
#include "filevideocapturer.h"
#include "rtspaudiocapturer.h"
#include "fileaudiocapturer.h"
#endif

#ifdef USE_X11
#include "screencapturer.h"
#include "windowcapturer.h"
#endif

#include "pc/video_track_source.h"

template<class T>
class TrackSource : public webrtc::VideoTrackSource {
public:
	static rtc::scoped_refptr<TrackSource> Create(const std::string & videourl, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
		std::unique_ptr<T> capturer = absl::WrapUnique(T::Create(videourl, opts, videoDecoderFactory));
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
						std::string devname;
						auto it = std::find(videoDeviceList.begin(), videoDeviceList.end(), name);
						if (it == videoDeviceList.end()) {
							devname = name;
						} else {
							devname = "videocap://";
							devname += std::to_string(i);
						}
						videoDeviceList.push_back(devname);
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

	static rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(const std::string & videourl, const std::map<std::string,std::string> & opts, const std::regex & publishFilter, rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource;
		if ((videourl.find("rtsp://") == 0) && (std::regex_match("rtsp://", publishFilter))) {
#ifdef HAVE_LIVE555
			videoSource = TrackSource<RTSPVideoCapturer>::Create(videourl,opts, videoDecoderFactory);
#endif	
		}
		else if ((videourl.find("file://") == 0) && (std::regex_match("file://", publishFilter)))
		{
#ifdef HAVE_LIVE555
			videoSource = TrackSource<FileVideoCapturer>::Create(videourl, opts, videoDecoderFactory);
#endif
		}
		else if ((videourl.find("rtp://") == 0) && (std::regex_match("rtp://", publishFilter)))
		{
#ifdef HAVE_LIVE555
			videoSource = TrackSource<RTPVideoCapturer>::Create(SDPClient::getSdpFromRtpUrl(videourl), opts, videoDecoderFactory);
#endif
		}		
		else if ((videourl.find("screen://") == 0) && (std::regex_match("screen://", publishFilter)))
		{
#ifdef USE_X11
			videoSource = TrackSource<ScreenCapturer>::Create(videourl, opts, videoDecoderFactory);
#endif	
		}
		else if ((videourl.find("window://") == 0) && (std::regex_match("window://", publishFilter)))
		{
#ifdef USE_X11
			videoSource = TrackSource<WindowCapturer>::Create(videourl, opts, videoDecoderFactory);
#endif	
		}
		else if (std::regex_match("videocap://",publishFilter)) {
			videoSource = TrackSource<VcmCapturer>::Create(videourl, opts, videoDecoderFactory);
		}
		return videoSource;
	}

	static rtc::scoped_refptr<webrtc::AudioSourceInterface> CreateAudioSource(const std::string & audiourl, 
							const std::map<std::string,std::string> & opts, 
							const std::regex & publishFilter, 
							rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory,
							rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderfactory,
							rtc::scoped_refptr<webrtc::AudioDeviceModule>   audioDeviceModule) {
		rtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource;

		if ( (audiourl.find("rtsp://") == 0) && (std::regex_match("rtsp://",publishFilter)) )
		{
	#ifdef HAVE_LIVE555
			audioDeviceModule->Terminate();
			audioSource = RTSPAudioSource::Create(audioDecoderfactory, audiourl, opts);
	#endif
		}
		else if ( (audiourl.find("file://") == 0) && (std::regex_match("file://",publishFilter)) )
		{
	#ifdef HAVE_LIVE555
			audioDeviceModule->Terminate();
			audioSource = FileAudioSource::Create(audioDecoderfactory, audiourl, opts);
	#endif
		}
		else if (std::regex_match("audiocap://",publishFilter)) 
		{
			audioDeviceModule->Init();
			int16_t num_audioDevices = audioDeviceModule->RecordingDevices();
			int16_t idx_audioDevice = -1;
			char name[webrtc::kAdmMaxDeviceNameSize] = {0};
			char id[webrtc::kAdmMaxGuidSize] = {0};			
			if (audiourl.find("audiocap://") == 0) {
				int deviceNumber = atoi(audiourl.substr(strlen("audiocap://")).c_str());
				RTC_LOG(INFO) << "audiourl:" << audiourl << " device number:" << deviceNumber;
				if (audioDeviceModule->RecordingDeviceName(deviceNumber, name, id) != -1)
				{
					idx_audioDevice = deviceNumber;
				}

			} else {
				for (int i = 0; i < num_audioDevices; ++i)
				{
					if (audioDeviceModule->RecordingDeviceName(i, name, id) != -1)
					{
						RTC_LOG(INFO) << "audiourl:" << audiourl << " idx_audioDevice:" << i << " " << name;
						if (audiourl == name)
						{
							idx_audioDevice = i;
							break;
						}
					}
				}
			}
			RTC_LOG(LS_ERROR) << "audiourl:" << audiourl << " idx_audioDevice:" << idx_audioDevice << "/" << num_audioDevices;
			if ( (idx_audioDevice >= 0) && (idx_audioDevice < num_audioDevices) )
			{
				audioDeviceModule->SetRecordingDevice(idx_audioDevice);
				cricket::AudioOptions opt;
				audioSource = peer_connection_factory->CreateAudioSource(opt);
			}
		}
		return audioSource;
	}
};

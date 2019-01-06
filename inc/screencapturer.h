/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** screencapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "rtc_base/thread.h"
#include "media/base/videocapturer.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"


class DesktopCapturer : public cricket::VideoCapturer, public rtc::Thread, public webrtc::DesktopCapturer::Callback  {
	public:
		DesktopCapturer(const std::map<std::string,std::string> & opts) : m_width(0), m_height(0) {
			if (opts.find("width") != opts.end()) {
				m_width = std::stoi(opts.at("width"));
			}	
			if (opts.find("height") != opts.end()) {
				m_height = std::stoi(opts.at("height"));
			}
		}
		virtual ~DesktopCapturer() {}

		// overide webrtc::DesktopCapturer::Callback
		virtual void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame);
		
		// overide rtc::Thread
		virtual void Run();

		// overide cricket::VideoCapturer
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format);
		virtual void Stop();
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) { return true; }
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	
	protected:
		std::unique_ptr<webrtc::DesktopCapturer> m_capturer;
		int                                      m_width;		
		int                                      m_height;		
};


class ScreenCapturer : public DesktopCapturer {
	public:
		ScreenCapturer(const std::string & url, const std::map<std::string,std::string> & opts) : DesktopCapturer(opts) {
			const std::string prefix("screen://");
			m_capturer = webrtc::DesktopCapturer::CreateScreenCapturer(webrtc::DesktopCaptureOptions::CreateDefault());
			if (m_capturer) {
				webrtc::DesktopCapturer::SourceList sourceList;
				if (m_capturer->GetSourceList(&sourceList)) {
					const std::string screen(url.substr(prefix.length()));
					if (screen.empty() == false) {
						for (auto source : sourceList) {
							RTC_LOG(LS_ERROR) << "ScreenCapturer source:" << source.id << " title:"<< source.title;
							if (atoi(screen.c_str()) == source.id) {
								m_capturer->SelectSource(source.id);
								break;
							}
						}
					}
				}
			}			
		}
};

class WindowCapturer : public DesktopCapturer {
	public:
		WindowCapturer(const std::string & url, const std::map<std::string,std::string> & opts) : DesktopCapturer(opts) {
			const std::string windowprefix("window://");
			if (url.find(windowprefix) == 0) {	
				m_capturer = webrtc::DesktopCapturer::CreateWindowCapturer(webrtc::DesktopCaptureOptions::CreateDefault());
			
				if (m_capturer) {
					webrtc::DesktopCapturer::SourceList sourceList;
					if (m_capturer->GetSourceList(&sourceList)) {
						const std::string windowtitle(url.substr(windowprefix.length()));
						for (auto source : sourceList) {
							RTC_LOG(LS_ERROR) << "WindowCapturer source:" << source.id << " title:"<< source.title;
							if (windowtitle == source.title) {
								m_capturer->SelectSource(source.id);
								break;
							}
						}
					}
				}
			}			
		}
};


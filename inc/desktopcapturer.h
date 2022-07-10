/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** screencapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <thread>

#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "media/base/video_common.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"

#include "VideoSource.h"

class DesktopCapturer : public VideoSource, public webrtc::DesktopCapturer::Callback  {
	public:
		DesktopCapturer(const std::map<std::string,std::string> & opts) : m_width(0), m_height(0) {
			if (opts.find("width") != opts.end()) {
				m_width = std::stoi(opts.at("width"));
			}	
			if (opts.find("height") != opts.end()) {
				m_height = std::stoi(opts.at("height"));
			}
		}
		bool Init() {
			return this->Start();
		}
		virtual ~DesktopCapturer() {
			this->Stop();
		}
		
		void CaptureThread();
		

		bool Start();
		void Stop();
		bool IsRunning() { return m_isrunning; }

		// overide webrtc::DesktopCapturer::Callback
		virtual void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame);
		
        int width() { return m_width;  }
        int height() { return m_height;  }        

	
	protected:
		std::thread                              m_capturethread;
		std::unique_ptr<webrtc::DesktopCapturer> m_capturer;
		int                                      m_width;		
		int                                      m_height;	
		bool                                     m_isrunning;
};





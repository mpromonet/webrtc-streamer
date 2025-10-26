/* ---------------------------------------------------------------------------
 * SPDX-License-Identifier: Unlicense
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <http://unlicense.org/>
 * -------------------------------------------------------------------------*/
#pragma once

#include "desktopcapturer.h"

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
		static ScreenCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
			std::unique_ptr<ScreenCapturer> capturer(new ScreenCapturer(url, opts));
			if (!capturer->Init()) {
				RTC_LOG(LS_WARNING) << "Failed to create WindowCapturer";
				return nullptr;
			}
			return capturer.release();
		}
};

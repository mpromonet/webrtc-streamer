#include <rfb/rfbclient.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include "VNCVideoCapturer.h"

#include "rtc_base/thread.h"
#include "rtc_base/logging.h"
#include "api/video_codecs/video_decoder.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"
#include "api/video/i420_buffer.h"
#include "libyuv/video_common.h"
#include "libyuv/convert.h"
#include <iostream>
#include "url.h"

static char * get_password (rfbClient *client) {
  VNCVideoCapturer* that = (VNCVideoCapturer *) rfbClientGetClientData(client, (void *) "this");
  return that->onGetPassword();
}

static void get_frame(rfbClient* client) {
  VNCVideoCapturer* that = (VNCVideoCapturer *) rfbClientGetClientData(client, (void *) "this");
  return that->onFrameBufferUpdate();
}

static void signal_handler(int sig) {
  if (sig == SIGINT) {
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Recieved SIGINT, exiting ...";
	exit(EXIT_FAILURE);
  }
}

char* VNCVideoCapturer::onGetPassword() {
  std::cout<< "Starting to get PASSWORD!!!" << std::endl;
	if (!url.password.length()) {
		if (getenv("VNCPASS") != NULL) {
			return strdup(getenv("VNCPASS"));
		}
	}

  return strdup((char* )url.password.c_str());
}

void VNCVideoCapturer::onClick(int x, int y, int buttonMask) {
	double xRatio = x / 1000.0, yRatio = y / 1000.0;
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Sending click!!! (" << xRatio << ',' << yRatio << ") with click: " << buttonMask;
	SendPointerEvent(client, (int) client->width * xRatio, (int) client->height * yRatio, buttonMask);
}

void VNCVideoCapturer::onPress(unsigned int code, bool down) {
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Sending key!!! (" << code << ',' << down << ")";
	SendKeyEvent(client, code, down);
}

void VNCVideoCapturer::onFrameBufferUpdate() {
	int i, j;
	rfbPixelFormat* pf = &client->format;
	int bpp = pf->bitsPerPixel/8;
	int row_stride = client->width*bpp;

	// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Parsing Frame with BPP: " << bpp;
	/* assert bpp=4 */
	if(bpp!=4) {
		onError("Got back VNC frame with bpp !== 4");
		return;
	}
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	int64_t ts = (now.tv_sec * 1000) + round(now.tv_nsec / 1.0e6);

	size_t size = row_stride * client->height;

	uint8_t * rgba_buffer = new uint8_t[client->width * client->height * 3];

	// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Parsing Height:Width - " << client->height << ":" << client->width;

	for(int j = 0, k = 0; j < client->height*row_stride; k += 1, j += row_stride) {
		for (int l = 0, i = 0; i < client->width * bpp; i += bpp, l+=3) {
			// It's assumed here that the pixls are 32 bits (bpp == 4), with 3 bytes
			// for RGB and the shift values for RGB are 16/8/0
			// https://tools.ietf.org/html/rfc6143#section-7.4
			int index = (k * client->width * 3) + l;
			rgba_buffer[index + 0] = client->frameBuffer[j + i + 0];
			rgba_buffer[index + 1] = client->frameBuffer[j + i + 1];
			rgba_buffer[index + 2] = client->frameBuffer[j + i + 2];
		}
	}


	rtc::scoped_refptr<webrtc::I420Buffer> I420buffer = webrtc::I420Buffer::Create(client->width, client->height);
	// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Transcoding frame ...";
	const int conversionResult = libyuv::RAWToI420(rgba_buffer, client->width * 3,
		(uint8_t*)I420buffer->DataY(), I420buffer->StrideY(),
		(uint8_t*)I420buffer->DataU(), I420buffer->StrideU(),
		(uint8_t*)I420buffer->DataV(), I420buffer->StrideV(),
		client->width, client->height
	);				
	// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Finished Transcoding Frame!!! :D";
					
	if (conversionResult >= 0) {
		webrtc::VideoFrame frame(I420buffer, 0, ts, webrtc::kVideoRotation_0);
		// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Sending Frame!!! :D";
		this->OnFrame(frame, frame.height(), frame.width());
	} else {
		RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "got back error decoding data" << conversionResult;
		onError("Got back Error Handling Frame!!");
	}

	delete[] rgba_buffer;
}

VNCVideoCapturer::VNCVideoCapturer(const std::string u): client(rfbGetClient(8,3,4))
{
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Trying to initialize VNC with: " << uri;
	url.ParseUrl(u);
}

bool VNCVideoCapturer::onStart() {
	client->GetPassword = get_password;
	client->FinishedFrameBufferUpdate = get_frame;

	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Trying to start VNC with: " << url.toString();

	if (url.isEmpty) {
		this->onError("Can not parse url: " + url.toString());
		return false;
	}

	if (!url.isDomainOf("rnfrst.com")) {
		this->onError("Access Denied");
		return false;
	}
	
	if (url.scheme != "vnc") {
		this->onError("The scheme needs to be vnc:" + url.scheme);
		return false;
	}

	const char *args[] = { "VNCVideoCapture", url.host.c_str() };
	int len = 2;
	rfbClientSetClientData(client, (void *) "this", (void *) this);
	if (!rfbInitClient(client, &len, (char **) args)) {
		this->onError("Something went wrong in initializing RFB client ");
		return false;
	}
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Initialized VNC Successfully: " << url.toString();
	return true;
}

void VNCVideoCapturer::onError(std::string str) {
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << str;
	this->Stop();
	// exit(EXIT_FAILURE);
}

VNCVideoCapturer::~VNCVideoCapturer()
{
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "~~~VNCVideoCapturer";
}

// overide rtc::Thread
void VNCVideoCapturer::Run()
{
	RTC_LOG(LS_ERROR) << "VNCVideoCapturer Frame capture started ...";
	if (!this->onStart()) {
		return;
	}
	signal(SIGINT, signal_handler);
	SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
	while (IsRunning()) {
		// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Capturing Frame ...";
		if (WaitForMessage(client, 50000) < 0) {
			RTC_LOG(LS_ERROR) << "VNCVideoCapturer Frame capture timed out";
			this->Stop();
			break;
		}
		if (!HandleRFBServerMessage(client)) {
			RTC_LOG(LS_ERROR) << "VNCVideoCapturer Frame capture cant handle message";
			this->Stop();
			break;
		}
	}
}

cricket::CaptureState VNCVideoCapturer::Start(const cricket::VideoFormat& format)
{
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Start ...";
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	return cricket::CS_RUNNING;
}

void VNCVideoCapturer::Stop()
{
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Stop ...";
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
}

bool VNCVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}
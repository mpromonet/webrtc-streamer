#include <rfb/rfbclient.h>
#include <signal.h>
#include <time.h>
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

char *host = "127.0.0.1";
int port = 5900;

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
  // remove vnc:// prefix from uri before passing along
  std::string url = uri.substr(6, std::string::npos);
  // if it contains password, remove it from the URL
  int index = url.find('@');
  if (index != std::string::npos) {
  	std::string password = url.substr(1, index - 1);
	std::cout<< "Parsed Password: " << password << std::endl;
    return strdup((char* )password.c_str());
  }

  if (getenv("VNCPASS") != NULL) {
    return strdup(getenv("VNCPASS"));
  }

  this->onError("No password defined");
  return NULL;
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
	int64_t ts = now.tv_sec * 1000;

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

VNCVideoCapturer::VNCVideoCapturer(const std::string u): client(rfbGetClient(8,3,4)), uri(u)
{
	client->GetPassword = get_password;
	client->FinishedFrameBufferUpdate = get_frame;

	// remove vnc:// prefix from uri before passing along
	std::string url = uri.substr(6, std::string::npos);
	// if it contains password, remove it from the URL
	auto index = url.find('@');
	if (index != std::string::npos) {
		url = url.substr(index + 1);
	}
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Trying to initialize VNC with: " << url;

	const char *args[] = { "VNCVideoCapture", url.c_str() };
	int len = 2;
	rfbClientSetClientData(client, (void *) "this", (void *) this);
	if (!rfbInitClient(client, &len, (char **) args)) {
		onError("Something went wrong in initializing RFB client ");
		return;
	}
	RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << "Initialized VNC Successfully: " << url;
}

void VNCVideoCapturer::onError(char str[]) {
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << str;
	exit(EXIT_FAILURE);
}

VNCVideoCapturer::~VNCVideoCapturer()
{
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "~~~VNCVideoCapturer";
}

// overide rtc::Thread
void VNCVideoCapturer::Run()
{
	RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Running ...";
	signal(SIGINT, signal_handler);
	while (IsRunning()) {
		// RTC_LOG(LS_VERBOSE) << __PRETTY_FUNCTION__ << "Capturing Frame ...";
		SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
		if (WaitForMessage(client, 50) < 0)
			break;
		if (!HandleRFBServerMessage(client))
			break;
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
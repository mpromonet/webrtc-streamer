#include <rfb/rfbclient.h>
#include <signal.h>
#include "VNCVideoCapturer.h"

#include "rtc_base/thread.h"
#include "rtc_base/logging.h"
#include "api/video_codecs/video_decoder.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"


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
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Recieved SIGINT, exiting ...";
	exit(EXIT_FAILURE);
  }
}

char* VNCVideoCapturer::onGetPassword() {
  if (getenv("VNCPASS") != NULL) {
    return strdup(getenv("VNCPASS"));
  }

  this->onError("No password defined");
  return NULL;
}

void VNCVideoCapturer::onFrameBufferUpdate() {
	int i, j;
	rfbPixelFormat* pf = &client->format;
	int bpp = pf->bitsPerPixel/8;
	int row_stride = client->width*bpp;

	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Parsing Frame with BPP: " << bpp;
	/* assert bpp=4 */
	if(bpp!=4 && bpp!=2) {
		onError("Got back VNC frame with bpp !== 2 or 4");
		return;
	}

	for(j=0;j<client->height*row_stride;j+=row_stride) {
		for(i=0;i<client->width*bpp;i+=bpp) {
			unsigned char* p = client->frameBuffer+j+i;
			unsigned int v;
			if(bpp==4)
				v=*(unsigned int*)p;
			else if(bpp==2)
				v=*(unsigned short*)p;
			else
				v=*(unsigned char*)p;
			// parse frame ...
		}
	}
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Captured Frame!!!";
}

VNCVideoCapturer::VNCVideoCapturer(const std::string & uri): client(rfbGetClient(8,3,4)) 
{
	client->GetPassword = get_password;
	client->FinishedFrameBufferUpdate = get_frame;

	// remove vnc:// prefix from uri before passing along
	std::string url = uri.substr(6, std::string::npos);
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Trying to initialize VNC with: " << url;

	const char *args[] = { "VNCVideoCapture", url.c_str() };
	int len = 2;
	if (!rfbInitClient(client, &len, (char **) args)) {
		onError("Something went wrong in initializing RFB client ");
		return;
	}
	rfbClientSetClientData(client, (void *) "this", (void *) this);
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Initialized VNC Successfully: " << url;
}

void VNCVideoCapturer::onError(char str[]) {
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << str;
	exit(EXIT_FAILURE);
}

VNCVideoCapturer::~VNCVideoCapturer()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "~~~VNCVideoCapturer";
}

// overide rtc::Thread
void VNCVideoCapturer::Run()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Running ...";
	signal(SIGINT, signal_handler);
	while (IsRunning()) {
		RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Capturing Frame ...";
		SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
		if (WaitForMessage(client, 50) < 0)
			break;
		if (!HandleRFBServerMessage(client))
			break;
	}
}

cricket::CaptureState VNCVideoCapturer::Start(const cricket::VideoFormat& format)
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Start ...";
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	return cricket::CS_RUNNING;
}

void VNCVideoCapturer::Stop()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Stop ...";
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
}

bool VNCVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}
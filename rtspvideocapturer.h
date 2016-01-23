/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
** 
** -------------------------------------------------------------------------*/

#ifndef RTSPVIDEOCAPTURER_H_
#define RTSPVIDEOCAPTURER_H_

#include <string.h>
#include <vector>

#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"
#include "webrtc/base/timeutils.h"
#ifdef HAVE_WEBRTC_VIDEO
#include "talk/media/webrtc/webrtcvideoframefactory.h"
#endif

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

class Callback
{
	public:
		virtual bool notifySession(const char* media, const char* codec) = 0;
		virtual bool notifyData(unsigned char* buffer, ssize_t size) = 0;
};


#define RTSP_CALLBACK(uri, resultCode, resultString) \
static void continueAfter ## uri ## Stub(RTSPClient* rtspClient, int resultCode, char* resultString) { static_cast<RTSPConnection*>(rtspClient)->continueAfter ## uri(resultCode, resultString); } \
void continueAfter ## uri (int resultCode, char* resultString) \
/**/
class RTSPConnection : public RTSPClient
{
	class SessionSink: public MediaSink 
	{
		public:
			static SessionSink* createNew(UsageEnvironment& env, Callback* callback) { return new SessionSink(env, callback); }

		private:
			SessionSink(UsageEnvironment& env, Callback* callback) : MediaSink(env), m_callback(callback) 
			{
				m_buffer[0] = 0;
				m_buffer[1] = 0;
				m_buffer[2] = 0;
				m_buffer[3] = 1;
			}

			static void afterGettingFrame(void* clientData, unsigned frameSize,
						unsigned numTruncatedBytes,
						struct timeval presentationTime,
						unsigned durationInMicroseconds)
			{
				static_cast<SessionSink*>(clientData)->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
			}
			
			void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
					 struct timeval presentationTime, unsigned durationInMicroseconds)
			{
				this->envir() << "NOTIFY size:" << frameSize << "\n";
				m_callback->notifyData(m_buffer, frameSize+4);
				this->continuePlaying();
			}

		private:
			virtual Boolean continuePlaying()
			{
				Boolean ret = False;
				if (fSource != NULL)
				{
					fSource->getNextFrame(m_buffer+4, sizeof(m_buffer)-4,
							afterGettingFrame, this,
							onSourceClosure, this);
					ret = True;
				}
				return ret;	
			}

		private:
			u_int8_t  m_buffer[1024*1024];
			Callback* m_callback; 	
	};
	
	public:
		RTSPConnection(Callback* callback, char const* rtspURL, int verbosityLevel = 255) 
						: m_callback(callback)
						, RTSPClient(*BasicUsageEnvironment::createNew(*BasicTaskScheduler::createNew()), rtspURL, verbosityLevel, NULL, 0, -1)
						, m_env(&this->envir())
						, m_stop(0)
		{
		}
		
		virtual ~RTSPConnection()
		{
			TaskScheduler* scheduler = &m_env->taskScheduler();
			m_env->reclaim();
			delete scheduler;
		}
				
		void setupNextSubsession() 
		{
			m_subSession = m_subSessionIter->next();
			if (m_subSession != NULL) 
			{
				if (!m_subSession->initiate()) 
				{
					*m_env << "Failed to initiate " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << m_env->getResultMsg() << "\n";
					this->setupNextSubsession();
				} 
				else 
				{					
					*m_env << "Initiated " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession\n";
				}

				this->sendSetupCommand(*m_subSession, continueAfterSETUPStub);
			}
			else
			{
				this->sendPlayCommand(*m_session, continueAfterPLAYStub);
			}
		}
				
		RTSP_CALLBACK(DESCRIBE,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to get a SDP description: " << resultString << "\n";
			}
			else
			{
				char* const sdpDescription = resultString;
				*m_env << "Got a SDP description:\n" << sdpDescription << "\n";
				m_session = MediaSession::createNew(*m_env, sdpDescription);
				m_subSessionIter = new MediaSubsessionIterator(*m_session);
				this->setupNextSubsession();  
			}
			delete[] resultString;
		}
		
		RTSP_CALLBACK(SETUP,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to SETUP: " << resultString << "\n";
			}
			else
			{				
				m_subSession->sink = SessionSink::createNew(*m_env, m_callback);
				if (m_subSession->sink == NULL) 
				{
					*m_env << "Failed to create a data sink for " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << m_env->getResultMsg() << "\n";
				}
				else
				{
					*m_env << "Created a data sink for the \"" << m_subSession << "\" subsession\n";
					m_subSession->sink->startPlaying(*(m_subSession->readSource()), NULL, NULL);
				}
				m_callback->notifySession(m_subSession->mediumName(), m_subSession->codecName());
			}
			delete[] resultString;
			this->setupNextSubsession();  
		}	
		
		RTSP_CALLBACK(PLAY,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to PLAY: " << resultString << "\n";
			}
			else
			{
				*m_env << "PLAY OK " << resultString << "\n";
			}
			delete[] resultString;
		}
		
		void mainloop()
		{
			this->sendDescribeCommand(continueAfterDESCRIBEStub); 
			m_env->taskScheduler().doEventLoop(&m_stop);
		}
		
		static void* MainLoop(void* client)
		{
			static_cast<RTSPConnection*>(client)->mainloop();
			pthread_exit(NULL);
		}
		
		void stop() { m_stop = 1; };
		
	protected:
		UsageEnvironment* m_env;
		MediaSession* m_session;                   
		MediaSubsession* m_subSession;             
		MediaSubsessionIterator* m_subSessionIter;
		Callback* m_callback; 	
		char m_stop;
};

class RTSPVideoCapturer : public cricket::VideoCapturer, public Callback
{
	public:
		RTSPVideoCapturer(const std::string & uri) : m_connection(this,uri.c_str())
		{
#ifdef HAVE_WEBRTC_VIDEO
			set_frame_factory(new cricket::WebRtcVideoFrameFactory());
#endif		
			std::vector<cricket::VideoFormat> formats;
			formats.push_back(cricket::VideoFormat(720, 576, cricket::VideoFormat::FpsToInterval(25), cricket::FOURCC_H264));
			SetSupportedFormats(formats);
		}
	  
		virtual ~RTSPVideoCapturer() 
		{
		}
		
		virtual bool notifySession(const char* media, const char* codec)
		{
		}
		
		virtual bool notifyData(unsigned char* buffer, ssize_t size) 
		{
			if (!IsRunning() || !GetCaptureFormat()) {
				return false;
			}

			cricket::CapturedFrame frame;
			frame.width = GetCaptureFormat()->width;
			frame.height = GetCaptureFormat()->height;
			frame.fourcc = GetCaptureFormat()->fourcc;
			frame.data_size = size;

			rtc::scoped_ptr<char[]> data(new char[size]);
			frame.data = data.get();
			memcpy(frame.data, buffer, size);

			SignalFrameCaptured(this, &frame);
			return true;
		}

		virtual cricket::CaptureState Start(const cricket::VideoFormat& format) 
		{
			SetCaptureFormat(&format);
			SetCaptureState(cricket::CS_RUNNING);
			pthread_create(&m_thid, NULL, RTSPConnection::MainLoop, &m_connection);
			return cricket::CS_RUNNING;
		}
	  
		virtual void Stop() {
			m_connection.stop();
			void * status = NULL;
			pthread_join(m_thid, &status);
			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CS_STOPPED);
		}
	  
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) 
		{
			fourccs->push_back(cricket::FOURCC_H264);
			return true;
		}
	  
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	  
	private:
		RTSPConnection m_connection;
		pthread_t m_thid;
		
};

#endif 


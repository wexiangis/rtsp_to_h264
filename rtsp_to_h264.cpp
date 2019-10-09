/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2019, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

// Forward function definitions:

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData, char const* reason);
  // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
  StreamClientState();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient: public RTSPClient {
public:
  static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
				  int verbosityLevel = 0,
				  char const* applicationName = NULL,
				  portNumBits tunnelOverHTTPPortNum = 0);

protected:
  ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
		int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
  virtual ~ourRTSPClient();

public:
  StreamClientState scs;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class DummySink: public MediaSink {
public:
  static DummySink* createNew(UsageEnvironment& env,
			      MediaSubsession& subsession, // identifies the kind of data that's being received
			      char const* streamId = NULL); // identifies the stream itself (optional)

private:
  DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
    // called only by "createNew()"
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
				struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			 struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  char* fStreamId;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL) {
  // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
  // to receive (even if more than stream uses the same "rtsp://" URL).
  RTSPClient* rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
  if (rtspClient == NULL) {
    env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
    return;
  }

  ++rtspClientCount;

  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
  // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
  // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE); 
}


// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
      delete[] resultString;
      break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.session == NULL) {
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
      break;
    } else if (!scs.session->hasSubsessions()) {
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void setupNextSubsession(RTSPClient* rtspClient) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
  
  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL) {
    if (!scs.subsession->initiate()) {
      env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    } else {
      env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
      if (scs.subsession->rtcpIsMuxed()) {
	env << "client port " << scs.subsession->clientPortNum();
      } else {
	env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
      }
      env << ")\n";

      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
      rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
  }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
      break;
    }

    env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
    if (scs.subsession->rtcpIsMuxed()) {
      env << "client port " << scs.subsession->clientPortNum();
    } else {
      env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
    }
    env << ")\n";

    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
    // after we've sent a RTSP "PLAY" command.)

    scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url());
      // perhaps use your own custom "MediaSink" subclass instead
    if (scs.subsession->sink == NULL) {
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
	  << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
    scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession 
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
				       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
  Boolean success = False;

  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData, char const* reason) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << *rtspClient << "Received RTCP \"BYE\"";
  if (reason != NULL) {
    env << " (reason:\"" << reason << "\")";
    delete[] reason;
  }
  env << " on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) { 
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
	Medium::close(subsession->sink);
	subsession->sink = NULL;

	if (subsession->rtcpInstance() != NULL) {
	  subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
	}

	someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

  if (--rtspClientCount == 0) {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
    exit(exitCode);
  }
}


// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
}


// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}


// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1000000

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId) {
  return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env),
    fSubsession(subsession) {
  fStreamId = strDup(streamId);
  fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink() {
  delete[] fReceiveBuffer;
  delete[] fStreamId;
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

Boolean DummySink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}


//---------------------------------------- 分割线 ----------------------------------------

extern int shm_create(char *path, int flag, int size, int isService, void **mem);
extern int shm_destroy(int id);

typedef struct{
    unsigned char type;
    unsigned char width[2];
    unsigned char height[2];
    unsigned char fps;
    unsigned char flag;
    unsigned char order;
    unsigned char len[4];
    unsigned char data[524276];
}ShmData_Struct;

static int shm_fd = 0;
static ShmData_Struct *shm_dat = NULL;

static char tar_file_name[128] = "test";
static bool slave_mode = 0;//从机模式,连接后从stdout吐帧数据,可用重定向'>>'来写到文件

static char shm_path[64] = "/tmp";
static char shm_flag[2] = "s";
static bool shm_mode = 0;

extern int h264_decode_sps(unsigned char * buf,unsigned int nLen,int *width,int *height,int *fps);
extern int h265_decode_sps(unsigned char * buf,unsigned int nLen,int *width,int *height,int *fps);

void DummySink::afterGettingFrame(
    unsigned frameSize, 
    unsigned numTruncatedBytes,
    struct timeval presentationTime, 
    unsigned /*durationInMicroseconds*/)
{
  static bool firstFrame = true;
  static int cI = 0, cB = 0, cP = 0;
  static bool isH264 = false;

  // We've just received a frame of data.  (Optionally) print out information about it:
  if(firstFrame)
  {
    if (fStreamId != NULL) 
      envir() << "Stream \"" << fStreamId << "\"; ";
    envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
    if (numTruncatedBytes > 0) 
      envir() << " (with " << numTruncatedBytes << " bytes truncated)";
    char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
    sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
    envir() << ".\tPresentation time: " << (unsigned)presentationTime.tv_sec << "." << uSecsStr;
    if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP())
      envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
    envir() << "\n";
  }

  //todo one frame
  //save to file
  if(!strcmp(fSubsession.mediumName(), "video"))
  {
    if(firstFrame)
    {
      if(!slave_mode)
      {
        if(strstr(fSubsession.codecName(), "265"))
        {
          strcpy(&tar_file_name[strlen(tar_file_name)], ".h265");
          FILE *fp = fopen(tar_file_name, "w");
          if(fp) fclose(fp);
          isH264 = false;
        }
        else if(strstr(fSubsession.codecName(), "264"))
        {
          strcpy(&tar_file_name[strlen(tar_file_name)], ".h264");
          FILE *fp = fopen(tar_file_name, "w");
          if(fp) fclose(fp);
          isH264 = true;
        }
      }
      firstFrame = False;
    }

    FILE *fp = NULL;
    if(slave_mode)
      fp = stdout;
    else
      fp = fopen(tar_file_name, "a+b");

    if(fp)
    {
      int frameType = 0;

      if(*((int*)fReceiveBuffer) != 0x1000000) // head == 00,00,00,01 ?
      {
        char head[4] = {0x00, 0x00, 0x00, 0x01};
        fwrite(head, 4, 1, fp);
      }
      else
        frameType = 4;
      fwrite(fReceiveBuffer, frameSize, 1, fp);

      if(!slave_mode)
        fclose(fp);

      if(isH264)
      {
        frameType = fReceiveBuffer[frameType]&0x1F;
        if(frameType == 7)
        {
          int width = 0, height = 0, fps = 0;
          if(h264_decode_sps(fReceiveBuffer,frameSize,&width,&height,&fps))
          {
            if(shm_dat)
            {
              shm_dat->type = 1;
              shm_dat->width[0] = width&0xFF;
              shm_dat->width[1] = (width>>8)&0xFF;
              shm_dat->height[0] = height&0xFF;
              shm_dat->height[1] = (height>>8)&0xFF;
              shm_dat->fps = fps;
            }
            envir() << "--> hit SPS frame: w/" << width
                    << " h/" << height
                    << " fps/" << fps
                    << " " << fSubsession.mediumName() 
                    << "/" << fSubsession.codecName()
                    << " I-frame/" << cI 
                    << " P-frame/" << cP 
                    << " B-frame/" << cB
                    << "\n";
          }
        }
        else if(frameType == 5)
        {
          cI += 1;
          cP = 0;
          cB = 0;
        }
        else if(frameType == 1)
          cP += 1;
      }
      else
      {
        frameType = (fReceiveBuffer[frameType]&0x7E)>>1;
        if(frameType == 33)
        {
          int width = 0, height = 0, fps = 0;
          if(h265_decode_sps(fReceiveBuffer,frameSize,&width,&height,&fps))
          {
            if(shm_dat)
            {
              shm_dat->type = 2;
              shm_dat->width[0] = width&0xFF;
              shm_dat->width[1] = (width>>8)&0xFF;
              shm_dat->height[0] = height&0xFF;
              shm_dat->height[1] = (height>>8)&0xFF;
              shm_dat->fps = fps;
            }
            envir() << "--> hit SPS frame: w/" << width
                    << " h/" << height
                    << " fps/" << fps
                    << " " << fSubsession.mediumName() 
                    << "/" << fSubsession.codecName()
                    << " I-frame/" << cI 
                    << " P-frame/" << cP 
                    << " B-frame/" << cB
                    << "\n";
          }
        }
        else if(frameType == 19)
        {
          cI += 1;
          cP = 0;
          cB = 0;
        }
        else if(frameType == 1)
          cP += 1;
      }
    }

    if(shm_dat)
    {
      shm_dat->flag = 1;
      shm_dat->len[0] = frameSize&0xFF;
      shm_dat->len[1] = (frameSize>>8)&0xFF;
      shm_dat->len[2] = (frameSize>>16)&0xFF;
      shm_dat->len[3] = (frameSize>>24)&0xFF;
      memcpy(shm_dat->data, fReceiveBuffer, frameSize);
      shm_dat->order++;
      shm_dat->flag = 0;
    }
  }
  // Then continue, to request the next frame of data:
  continuePlaying();
}

char eventLoopWatchVariable = 0;

void usage(UsageEnvironment& env, char const* progName)
{
  env << "\n";
  env << "Usage:\n";
  env << "  " << progName << " <option> <rtsp://usr:pwd@ip:port/path>\n";
  env << "\n";
  env << "Option:\n";
  env << "  -f fileName : write h264/h265 stream to file (default save to ./test.h26x)\n";
  env << "  -slave : write h264/h265 stream to stdout\n";
  env << "  -shm : backup h264/h265 data to share mem\n";
  env << "         total size : 512*1024=524288 bytes\n";
  env << "         ---------- format ----------\n";
  env << "         offset len : describe\n";
  env << "          [0]    1  : type 0/unknow 1/h264 2/h265\n";
  env << "          [1]    2  : width (Little-Endian)\n";
  env << "          [3]    2  : height (Little-Endian)\n";
  env << "          [5]    1  : fps\n";
  env << "          [6]    1  : flag 0/free 1/writing\n";
  env << "          [7]    1  : order loop 0~255\n";
  env << "          [8]    4  : data len (Little-Endian)\n";
  env << "          [12] 524276 : data\n";
  env << "  -shm_path path : share mem ipc_path (default: " << shm_path << ")\n";
  env << "  -shm_flag id : share mem ipc_flag (default: '" << shm_flag << "')\n";
  env << "\n";
  env << "Example:\n";
  env << "  " << progName << " rtsp://192.168.1.2/test\n";
  env << "  " << progName << " rtsp://user:1234@192.168.1.2:554/test\n";
  env << "  " << progName << " rtsp://192.168.1.2/test -f ./test\n";
  env << "  " << progName << " rtsp://192.168.1.2/test -slave >> ./test.h264\n";
  env << "\n";
}

int main(int argc, char** argv)
{
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

  // We need at least one "rtsp://" URL argument:
  if (argc < 2)
  {
    usage(*env, argv[0]);
    return 1;
  }

  // 解析传参
  char *param;
  int i;

  for(i = 1; i < argc; i++)
  {
    param = argv[i];
    
    if(strncmp(param, "-f", 2) == 0 && i + 1 < argc)
    {
      i += 1;
      memset(tar_file_name, 0, sizeof(tar_file_name));
      strcpy(tar_file_name, argv[i]);
    }
    else if(strncmp(param, "-sh", 2) == 0 && i + 1 < argc)
    {
      i += 1;
      memset(tar_file_name, 0, sizeof(tar_file_name));
      strcpy(tar_file_name, argv[i]);
    }
    else if(strncmp(param, "-shm_path", 9) == 0 && i + 1 < argc)
    {
      i += 1;
      memset(shm_path, 0, sizeof(shm_path));
      strcpy(shm_path, argv[i]);
    }
    else if(strncmp(param, "-shm_flag", 7) == 0 && i + 1 < argc)
    {
      i += 1;
      shm_flag[0] = argv[i][0];
    }
    else if(strncmp(param, "-shm", 3) == 0)
    {
      shm_mode = true;
    }
    else if(strstr(param, "-?") || strstr(param, "--help"))
    {
      usage(*env, argv[0]);
      return 1;
    }
    else
    {
      openURL(*env, argv[0], argv[i]);
    }
  }

  //共享内存准备
  if(shm_mode)
  {
    *env << "shm: size " << (int)sizeof(ShmData_Struct) 
      << " path " << shm_path 
      << " flag '" << shm_flag << "'\n";
    shm_fd = shm_create(shm_path, shm_flag[0], sizeof(ShmData_Struct), 1, (void**)&shm_dat);
  }

  // There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
  // for (int i = 1; i <= argc-1; ++i) {
  //   openURL(*env, argv[0], argv[i]);
  // }

  // All subsequent activity takes place within the event loop:
  env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
    // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.

  return 0;

  // If you choose to continue the application past this point (i.e., if you comment out the "return 0;" statement above),
  // and if you don't intend to do anything more with the "TaskScheduler" and "UsageEnvironment" objects,
  // then you can also reclaim the (small) memory used by these objects by uncommenting the following code:
  /*
    env->reclaim(); env = NULL;
    delete scheduler; scheduler = NULL;
  */
}


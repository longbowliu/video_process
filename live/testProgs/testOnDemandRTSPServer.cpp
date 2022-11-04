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
// Copyright (c) 1996-2022, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// main program

#include "liveMedia.hh"

#include "BasicUsageEnvironment.hh"
#include "announceURL.hh"

#include "H264LiveVideoServerMediaSubssion.cpp"
// #include "H264FramedLiveSource.cpp"

 UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = False;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;
 
static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName); // forward


int main(int argc, char** argv) {

	
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  // Create the RTSP server:
#ifdef SERVER_USE_TLS
  // Serve RTSPS: RTSP over a TLS connection:
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 322, authDB);
#else
  // Serve regular RTSP (over a TCP connection):
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
#endif
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }
#ifdef SERVER_USE_TLS
#ifndef STREAM_USING_SRTP
#define STREAM_USING_SRTP True
#endif
  rtspServer->setTLSState(PATHNAME_TO_CERTIFICATE_FILE, PATHNAME_TO_PRIVATE_KEY_FILE,
			  STREAM_USING_SRTP);
#endif

  char const* descriptionString
    = "Session streamed by \"testOnDemandRTSPServer\"";

  // Set up each of the possible streams that can be served by the
  // RTSP server.  Each such stream is implemented using a
  // "ServerMediaSession" object, plus one or more
  // "ServerMediaSubsession" objects for each audio/video substream.

  
#if 1
  // A H.264 live elementary stream:
  {//这里是自己的ServerMediaSubssion
  
        //这个是输出缓冲区的大小，要设置到比任意一帧h264 大
 	 OutPacketBuffer::maxSize = 2000000;
    char const* streamName = "h264Live";
	
    char const* inputFileName = "live";

    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    sms->addSubsession(H264LiveVideoServerMediaSubssion
		       ::createNew(*env,reuseFirstSource));
    rtspServer->addServerMediaSession(sms);
 
    announceStream(rtspServer, sms, streamName, inputFileName);
  }
#endif

#if 0
  // A H.264 video elementary stream:
  {
    char const* streamName = "h264ESVideoTest";
    char const* inputFileName = "/tmp/fifo";
    inputFileName =(const char*) encoder_->encoded_frame;
    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);

    sms->addSubsession(H264VideoFileServerMediaSubsession
		       ::createNew(*env, inputFileName, reuseFirstSource));
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName, inputFileName);
  }
#endif

  // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
  // Try first with the default HTTP port (80), and then with the alternative HTTP
  // port numbers (8000 and 8080).

#ifdef SERVER_USE_TLS
  // (Attempt to) use the default HTTPS port (443) instead:
  char const* httpProtocolStr = "HTTPS";
  if (rtspServer->setUpTunnelingOverHTTP(443)) {
#else
  char const* httpProtocolStr = "HTTP";
  if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
#endif
    *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-" << httpProtocolStr << " tunneling.)\n";
  } else {
    *env << "\n(RTSP-over-" << httpProtocolStr << " tunneling is not available.)\n";
  }

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName) {
  UsageEnvironment& env = rtspServer->envir();

  env << "\n\"" << streamName << "\" stream, from the file \""
      << inputFileName << "\"\n";
  announceURL(rtspServer, sms);
}

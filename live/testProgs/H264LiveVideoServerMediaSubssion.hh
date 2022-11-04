#ifndef _H264_LIVE_VIDEO_SERVER_MEDIA_SUBSESSION_HH
 
#define _H264_LIVE_VIDEO_SERVER_MEDIA_SUBSESSION_HH
 
#include "OnDemandServerMediaSubsession.hh"
// #include "H264FramedLiveSource.hh"
#include"v4l2_camera.h"
 
 
 
class H264LiveVideoServerMediaSubssion : public OnDemandServerMediaSubsession 
{
 
public:
	static H264LiveVideoServerMediaSubssion* createNew(UsageEnvironment& env, Boolean reuseFirstSource);
	 v4l2Camera * cam =NULL ;
 
protected:
 
	H264LiveVideoServerMediaSubssion(UsageEnvironment& env,Boolean reuseFirstSource);
	~H264LiveVideoServerMediaSubssion();
 
protected: // redefined virtual functions
	FramedSource* createNewStreamSource(unsigned clientSessionId,unsigned& estBitrate);
	RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                    unsigned char rtpPayloadTypeIfDynamic,
				    FramedSource* inputSource);
public:
 
};
 
#endif
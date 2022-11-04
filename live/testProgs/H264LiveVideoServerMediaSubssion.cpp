 
#include "H264LiveVideoServerMediaSubssion.hh"
#include "H264FramedLiveSource.cpp"
#include "H264VideoStreamFramer.hh"
#include "H264VideoRTPSink.hh"
 
H264LiveVideoServerMediaSubssion* H264LiveVideoServerMediaSubssion::createNew(UsageEnvironment& env, Boolean reuseFirstSource)
{
	
	return new H264LiveVideoServerMediaSubssion(env, reuseFirstSource);
}
 
H264LiveVideoServerMediaSubssion::H264LiveVideoServerMediaSubssion(UsageEnvironment& env,Boolean reuseFirstSource)
: OnDemandServerMediaSubsession(env,reuseFirstSource)
{
	 printf("LLB:\n");
	cam = new v4l2Camera("/dev/video0",640,360);
}
 
H264LiveVideoServerMediaSubssion::~H264LiveVideoServerMediaSubssion()
{
 
}
 bool justOnce = true;
FramedSource* H264LiveVideoServerMediaSubssion::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
{
	//创建视频源,参照H264VideoFileServerMediaSubsession
	if(justOnce){

	}
	H264FramedLiveSource* liveSource = H264FramedLiveSource::createNew(envir(),cam);
	if (liveSource == NULL)
	{
		return NULL;
	}
	// Create a framer for the Video Elementary Stream:
	// return H264VideoStreamFramer::createNew(envir(), liveSource);

	return H264VideoStreamDiscreteFramer::createNew(envir(), liveSource);
 
}
 
RTPSink* H264LiveVideoServerMediaSubssion
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
 

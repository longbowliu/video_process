#include <liveMedia.hh>
#include"v4l2_camera.h"
#include <queue>
struct raw_ts
	{
		
		Raw_data raw;
		timeval ts;
	};
	

	
class H264FramedLiveSource : public FramedSource
{
 
public:
	static H264FramedLiveSource* createNew(UsageEnvironment& env, v4l2Camera  *cam_source);
	// redefined virtual functions
	virtual unsigned maxFrameSize() const;
	v4l2Camera * cam_source =NULL ;
 
protected:
 
	H264FramedLiveSource(UsageEnvironment& en,v4l2Camera * camv);
 
	 virtual ~H264FramedLiveSource();
 
private:
	virtual void doGetNextFrame();

	
	std::queue<raw_ts> raw_ts_queue;
	
 
protected:
	
	// static TestFromFile * pTest;
 
};
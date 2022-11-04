#include "H264FramedLiveSource.hh"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include "x264_encoder.cpp"

#include <thread>
#include <mutex>
int count =0;
int encode_len = 0;






 void capture_image(v4l2Camera *camera,std::queue<raw_ts> & raw_ts_queue)
{
    while (true)
    {
        // Get the image as cv Mat type
        // auto start = std::chrono::system_clock::now();
		printf("try\n");
        camera->capture();
		delete( v4l2Camera::rt.start );
		printf("try2\n");
		timeval cur_ts = camera->buffer_ts;
		printf("try3\n");
        raw_ts_queue.push({camera->rt,cur_ts});
        // sleep(10);
		printf("hi , %d \t",count++);
        // cv::Mat cur_image = camera.raw_input.clone();
        // timeval cur_ts = camera.buffer_ts;
        // mat_ts_queue.push({cur_image,cur_ts});

        // auto end2 = std::chrono::system_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end2-start).count();
        // std::cout<<"capture image time:"<< duration<<std::endl;
    }
        
}



H264FramedLiveSource::H264FramedLiveSource(UsageEnvironment& env,v4l2Camera * cam)
: FramedSource(env)
{
	printf("44444444444444444444\n");
	cam_source = cam;
	
	std::thread th0(capture_image, cam_source,std::ref(raw_ts_queue));
	th0.detach();
	
}
 
H264FramedLiveSource* H264FramedLiveSource::createNew(UsageEnvironment& env ,v4l2Camera * cam)
{
	H264FramedLiveSource* newSource = new H264FramedLiveSource(env,cam);
	return newSource;
}
 
H264FramedLiveSource::~H264FramedLiveSource()
{
 
}
unsigned H264FramedLiveSource::maxFrameSize() const
{
	//printf("wangmaxframesize %d %s\n",__LINE__,__FUNCTION__);
	//这里返回本地h264帧数据的最大长度
	return 1920*1080*3;
}
 
void H264FramedLiveSource::doGetNextFrame()
{
        //这里读取本地的帧数据，就是一个memcpy(fTo,XX,fMaxSize),要确保你的数据不丢失，即fMaxSize要大于等于本地帧缓存的大小，关键在于上面的maxFrameSize() 虚函数的实现
    //     fFrameSize = XXXgeth264Frame(fTo,  fMaxSize);
    //     printf("read dat befor %d %s fMaxSize %d ,fFrameSize %d  \n",__LINE__,__FUNCTION__,fMaxSize,fFrameSize);
		
	// if (fFrameSize == 0) {
	// 	handleClosure();
	// 	return;
	//   }
	Raw_data rd;
	if(!raw_ts_queue.empty()){
		timeval image_ts = raw_ts_queue.front().ts;
		rd = raw_ts_queue.front().raw;
		printf("Sorry empty!\n");
	}
	fFrameSize = rd.length;
    if (fFrameSize > fMaxSize)
    {
        fNumTruncatedBytes = fFrameSize - fMaxSize;
        fFrameSize = fMaxSize;
        envir()<<"frame size "<<fFrameSize<<" MaxSize size "<<fMaxSize<<"fNumTruncatedBytes\n";
    }
    else
    {
        fNumTruncatedBytes = 0;
    }
 
 
    if(fFrameSize!=0){
        //把得到的实时数据复制进输出端
        memmove(fTo, rd.start, rd.length);
    }


 
	  //设置时间戳
    //   printf("o");
	  gettimeofday(&fPresentationTime, NULL);
 
	  // Inform the reader that he has data:
	  // To avoid possible infinite recursion, we need to return to the event loop to do this:
	  nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
					(TaskFunc*)FramedSource::afterGetting, this);
 
	// }
	
	return;
}
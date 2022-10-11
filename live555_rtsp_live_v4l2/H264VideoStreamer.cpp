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
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**********/
// Copyright (c) 1996-2022, Live Networks, Inc.  All rights reserved
// A test program that reads a H.264 Elementary Stream video file
// and streams it using RTP
// main program
//
// NOTE: For this application to work, the H.264 Elementary Stream video file *must* contain SPS and PPS NAL units,
// ideally at or near the start of the file.  These SPS and PPS NAL units are used to specify 'configuration' information
// that is set in the output stream's SDP description (by the RTSP server that is built in to this application).
// Note also that - unlike some other "*Streamer" demo applications - the resulting stream can be received only using a
// RTSP client (such as "openRTSP")

#include <liveMedia.hh>

#include <BasicUsageEnvironment.hh>
// #include "announceURL.hh"
#include <GroupsockHelper.hh>
#include <H264FramedLiveSource.hh>
#include <fstream>

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

UsageEnvironment* env;
bool useStream = true;
char const* inputFileName = "test.h264";

H264VideoStreamFramer* videoSource;
RTPSink* videoSink;
/*摄像头相关的全局变量声明区域*/
#define UVC_VIDEO_DEVICE "/dev/video2"  /*UVC摄像头设备节点*/
int uvc_video_fd; /*存放摄像头设备节点的文件描述符*/
unsigned char *video_memaddr_buffer[4]; /*存放的是摄像头映射出来的缓冲区首地址*/
int Image_Width;  /*图像的宽度*/
int Image_Height; /*图像的高度*/
/*X264编码器相关的全局变量声明区域*/
unsigned char *h264_buf=NULL;
// typedef struct
// {
//   x264_param_t *param;
//   x264_t *handle;
//   x264_picture_t *picture; //说明一个视频序列中每帧特点
//   x264_nal_t *nal;
// }Encoder;
Encoder en;
FILE *h264_fp;

void play(); // forward

/*函数声明区域*/
void X264_close_encoder(void); //关闭解码器
 
 
/*
函数功能: 处理退出的信号
*/
void exit_sighandler(int sig)
{
	/*关闭视频文件*/
	fclose(h264_fp);
	
	//关闭摄像头
	close(uvc_video_fd);
	
	//释放缓冲区
	free(h264_buf);
		
	//退出进程
	exit(1);
}
 
 
/*设置视频录制相关参数*/
static int x264_param_apply_preset(x264_param_t *param, const char *preset)
{
    char *end;
    int i = strtol( preset, &end, 10 );
    if( *end == 0 && i >= 0 && i < sizeof(x264_preset_names)/sizeof(*x264_preset_names)-1 )
        preset = x264_preset_names[i];
	/*快4*/
    if( !strcasecmp( preset, "ultrafast" ) )
    {
        param->i_frame_reference = 1;
        param->i_scenecut_threshold = 0;
        param->b_deblocking_filter = 0;
        param->b_cabac = 0;
        param->i_bframe = 0;
        param->analyse.intra = 0;
        param->analyse.inter = 0;
        param->analyse.b_transform_8x8 = 0;
        param->analyse.i_me_method = X264_ME_DIA;
        param->analyse.i_subpel_refine = 0;
        param->rc.i_aq_mode = 0;
        param->analyse.b_mixed_references = 0;
        param->analyse.i_trellis = 0;
        param->i_bframe_adaptive = X264_B_ADAPT_NONE;
        param->rc.b_mb_tree = 0;
        param->analyse.i_weighted_pred = X264_WEIGHTP_NONE;
        param->analyse.b_weighted_bipred = 0;
        param->rc.i_lookahead = 0;
    }
	/*快3*/
    else if( !strcasecmp( preset, "superfast" ) )
    {
        param->analyse.inter = X264_ANALYSE_I8x8|X264_ANALYSE_I4x4;
        param->analyse.i_me_method = X264_ME_DIA;
        param->analyse.i_subpel_refine = 1;
        param->i_frame_reference = 1;
        param->analyse.b_mixed_references = 0;
        param->analyse.i_trellis = 0;
        param->rc.b_mb_tree = 0;
        param->analyse.i_weighted_pred = X264_WEIGHTP_SIMPLE;
        param->rc.i_lookahead = 0;
    }
	/*快2*/
    else if( !strcasecmp( preset, "veryfast" ) )
    {
        param->analyse.i_me_method = X264_ME_HEX;
        param->analyse.i_subpel_refine = 2;
        param->i_frame_reference = 1;
        param->analyse.b_mixed_references = 0;
        param->analyse.i_trellis = 0;
        param->analyse.i_weighted_pred = X264_WEIGHTP_SIMPLE;
        param->rc.i_lookahead = 10;
    }
	/*快1*/
    else if( !strcasecmp( preset, "faster" ) )
    {
        param->analyse.b_mixed_references = 0;
        param->i_frame_reference = 2;
        param->analyse.i_subpel_refine = 4;
        param->analyse.i_weighted_pred = X264_WEIGHTP_SIMPLE;
        param->rc.i_lookahead = 20;
    }
	/*快速*/
    else if( !strcasecmp( preset, "fast" ) )
    {
        param->i_frame_reference = 2;
        param->analyse.i_subpel_refine = 6;
        param->analyse.i_weighted_pred = X264_WEIGHTP_SIMPLE;
        param->rc.i_lookahead = 30;
    }
	/*中等*/
    else if( !strcasecmp( preset, "medium" ) )
    {
        /* Default is medium */
    }
	/*慢速*/
    else if( !strcasecmp( preset, "slow" ) )
    {
        param->analyse.i_me_method = X264_ME_UMH;
        param->analyse.i_subpel_refine = 8;
        param->i_frame_reference = 5;
        param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
        param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;
        param->rc.i_lookahead = 50;
    }
    else if( !strcasecmp( preset, "slower" ) )
    {
        param->analyse.i_me_method = X264_ME_UMH;
        param->analyse.i_subpel_refine = 9;
        param->i_frame_reference = 8;
        param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
        param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;
        param->analyse.inter |= X264_ANALYSE_PSUB8x8;
        param->analyse.i_trellis = 2;
        param->rc.i_lookahead = 60;
    }
    else if( !strcasecmp( preset, "veryslow" ) )
    {
        param->analyse.i_me_method = X264_ME_UMH;
        param->analyse.i_subpel_refine = 10;
	}
    else
    {
        return -1;
    }
 
    return 0;
}
 
 
/*开始视频压缩*/
void compress_begin(Encoder *en, int width, int height) 
{
	en->param = (x264_param_t *) malloc(sizeof(x264_param_t));
	en->picture = (x264_picture_t *) malloc(sizeof(x264_picture_t));
 
	x264_param_default(en->param); //编码器默认设置
 
	/*订制编码器压缩的性能*/
	x264_param_apply_preset(en->param,"medium");
	en->param->i_width = width; //设置图像宽度
	en->param->i_height = height; //设置图像高度
	if((en->handle = x264_encoder_open(en->param)) == 0)
	{
		return;
	}
 
	x264_picture_alloc(en->picture, X264_CSP_I420, en->param->i_width,en->param->i_height);
	en->picture->img.i_csp = X264_CSP_I420;
	en->picture->img.i_plane = 3;
}
 
/*结束压缩*/
void compress_end(Encoder *en)
{
	if (en->picture)
	{
		x264_picture_clean(en->picture);
		free(en->picture);
		en->picture = 0;
	}
	
	if(en->param)
	{
		free(en->param);
		en->param = 0;
	}
	if(en->handle)
	{
		x264_encoder_close(en->handle);
	}
	free(en);
}
 
 
/*初始化编码器*/
void X264_init_encoder(int width,int height)
{
	compress_begin(&en,width,height);
	h264_buf=(uint8_t *)malloc(sizeof(uint8_t)*width*height*3);
	if(h264_buf==NULL)printf("X264缓冲区申请失败!\n");
}
 
 
/*压缩一帧数据*/
int compress_frame(Encoder *en, int type, uint8_t *in, uint8_t *out) {
	x264_picture_t pic_out;
	int nNal = 0;
	int result = 0;
	int i = 0 , j = 0 ;
	uint8_t *p_out = out;
	en->nal=NULL;
	uint8_t *p422;
 
	char *y = (char*)en->picture->img.plane[0];
	char *u = (char*)en->picture->img.plane[1];
	char *v = (char*)en->picture->img.plane[2];
 
 
//
	int widthStep422 = en->param->i_width * 2;
	for(i = 0; i < en->param->i_height; i += 2)
	{
		p422 = in + i * widthStep422;
		for(j = 0; j < widthStep422; j+=4)
		{
			*(y++) = p422[j];
			*(u++) = p422[j+1];
			*(y++) = p422[j+2];
		}
		p422 += widthStep422;
		for(j = 0; j < widthStep422; j+=4)
		{
			*(y++) = p422[j];
			*(v++) = p422[j+3];
			*(y++) = p422[j+2];
		}
	}
 
	switch (type) {
	case 0:
		en->picture->i_type = X264_TYPE_P;
		break;
	case 1:
		en->picture->i_type = X264_TYPE_IDR;
		break;
	case 2:
		en->picture->i_type = X264_TYPE_I;
		break;
	default:
		en->picture->i_type = X264_TYPE_AUTO;
		break;
	}
 
	/*开始264编码*/
	if (x264_encoder_encode(en->handle, &(en->nal), &nNal, en->picture,
			&pic_out) < 0) {
		return -1;
	}
	en->picture->i_pts++;
 
 
	for (i = 0; i < nNal; i++) {
		memcpy(p_out, en->nal[i].p_payload, en->nal[i].i_payload);
		p_out += en->nal[i].i_payload;
		result += en->nal[i].i_payload;
	}
 
	return result;
	//return nNal;
}
 
//编码并写入一帧数据
void encode_frame(uint8_t *yuv_frame)
{
	int h264_length = 0;
	//压缩一帧数据
	h264_length = compress_frame(&en, -1, yuv_frame, h264_buf);
	if(h264_length > 0)
	{
		printf("h264_length=%d\n",h264_length);
		//写入视频文件
		fwrite(h264_buf, h264_length,1,h264_fp);
	}
}
 
 
/*
函数功能: UVC摄像头初始化
返回值: 0表示成功
*/
int UVCvideoInit(void)
{
	/*1. 打开摄像头设备*/
	uvc_video_fd=open(UVC_VIDEO_DEVICE,O_RDWR);
	if(uvc_video_fd<0)
	{
		printf("%s 摄像头设备打开失败!\n",UVC_VIDEO_DEVICE);
		return -1;
	}
	
	/*2. 设置摄像头的属性*/
	struct v4l2_format format;
	memset(&format,0,sizeof(struct v4l2_format));
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*表示视频捕获设备*/
	format.fmt.pix.width=320;  /*预设的宽度*/
	format.fmt.pix.height=240; /*预设的高度*/
	format.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV; /*预设的格式*/
	format.fmt.pix.field=V4L2_FIELD_ANY; /*系统自动设置: 帧属性*/
	if(ioctl(uvc_video_fd,VIDIOC_S_FMT,&format)) /*设置摄像头的属性*/
	{
		printf("摄像头格式设置失败!\n");
		return -2;
	}
	
	Image_Width=format.fmt.pix.width;
	Image_Height=format.fmt.pix.height;
		
	printf("摄像头实际输出的图像尺寸:x=%d,y=%d\n",format.fmt.pix.width,format.fmt.pix.height);
	if(format.fmt.pix.pixelformat==V4L2_PIX_FMT_YUYV)
	{
		printf("当前摄像头支持YUV格式图像输出!\n");
	}
	else
	{
		printf("当前摄像头不支持YUV格式图像输出!\n");
		return -3;
	}
 
	/*3. 请求缓冲区: 申请摄像头数据采集的缓冲区*/
	struct v4l2_requestbuffers req_buff;
	memset(&req_buff,0,sizeof(struct v4l2_requestbuffers));
	req_buff.count=4; /*预设要申请4个缓冲区*/
	req_buff.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
	req_buff.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
	if(ioctl(uvc_video_fd,VIDIOC_REQBUFS,&req_buff)) /*申请缓冲区*/
	{
		printf("申请摄像头数据采集的缓冲区失败!\n");
		return -4;
	}
	printf("摄像头缓冲区申请的数量: %d\n",req_buff.count);
 
	/*4. 获取缓冲区的详细信息: 地址,编号*/
	struct v4l2_buffer buff_info;
	memset(&buff_info,0,sizeof(struct v4l2_buffer));
	int i;
	for(i=0;i<req_buff.count;i++)
	{
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
		if(ioctl(uvc_video_fd,VIDIOC_QUERYBUF,&buff_info)) /*获取缓冲区的详细信息*/
		{
			printf("获取缓冲区的详细信息失败!\n");
			return -5;
		}
		/*根据摄像头申请缓冲区信息: 使用mmap函数将内核的地址映射到进程空间*/
		video_memaddr_buffer[i]=( unsigned char *)mmap(NULL,buff_info.length,PROT_READ|PROT_WRITE,MAP_SHARED,uvc_video_fd,buff_info.m.offset); 
		if(video_memaddr_buffer[i]==NULL)
		{
			printf("缓冲区映射失败!\n");
			return -6;
		}
	}
 
	/*5. 将缓冲区放入采集队列*/
	memset(&buff_info,0,sizeof(struct v4l2_buffer));
	for(i=0;i<req_buff.count;i++)
	{
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.index=i; /*缓冲区的节点编号*/
		buff_info.memory=V4L2_MEMORY_MMAP; /*支持mmap内存映射*/
		if(ioctl(uvc_video_fd,VIDIOC_QBUF,&buff_info)) /*根据节点编号将缓冲区放入队列*/
		{
			printf("根据节点编号将缓冲区放入队列失败!\n");
			return -7;
		}
	}
 
	/*6. 启动摄像头数据采集*/
	int Type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(uvc_video_fd,VIDIOC_STREAMON,&Type))
	{
		printf("启动摄像头数据采集失败!\n");
		return -8;
	}
	return 0;
}
 
 
/*
函数功能: 采集摄像头的数据,并进行处理
*/
void *pthread_video_Data_Handler(void *dev)
{
	/*循环采集摄像头的数据*/
	struct pollfd fds;
	fds.fd=uvc_video_fd;
	fds.events=POLLIN;
 
	struct v4l2_buffer buff_info;
	memset(&buff_info,0,sizeof(struct v4l2_buffer));
	int index=0; /*表示当前缓冲区的编号*/
	
	printf("摄像头开始传输数据.......\n");
	while(1)
	{
		/*1. 等待摄像头采集数据*/
		poll(&fds,1,-1); 
 
		/*2. 取出一帧数据: 从采集队列里面取出一个缓冲区*/
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;   /*视频捕获设备*/
		ioctl(uvc_video_fd,VIDIOC_DQBUF,&buff_info); /*从采集队列取出缓冲区*/
		index=buff_info.index;
		//printf("采集数据的缓冲区的编号:%d\n",index);
 
		/*3. 处理数据: 进行H264编码*/
		//video_memaddr_buffer[index]; /*当前存放数据的缓冲区地址*/
		
		/*编码一帧数据*/
		encode_frame(video_memaddr_buffer[index]);
 
		/*4. 将缓冲区再次放入采集队列*/
		buff_info.memory=V4L2_MEMORY_MMAP; 	/*支持mmap内存映射*/
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.index=index; /*缓冲区的节点编号*/
		ioctl(uvc_video_fd,VIDIOC_QBUF,&buff_info); /*根据节点编号将缓冲区放入队列*/
	}
}
int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  // Create 'groupsocks' for RTP and RTCP:
  struct sockaddr_storage destinationAddress;
  destinationAddress.ss_family = AF_INET;
  ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*env);
  // Note: This is a multicast address.  If you wish instead to stream
  // using unicast, then you should use the "testOnDemandRTSPServer"
  // test program - not this test program - as a model.

  const unsigned short rtpPortNum = 18888;
  const unsigned short rtcpPortNum = rtpPortNum+1;
  const unsigned char ttl = 255;

  const Port rtpPort(rtpPortNum);
  const Port rtcpPort(rtcpPortNum);


  if( useStream ){
    inputFileName = "/tmp/fifo";
    FILE * h264_fp_buf=fopen("/home/demo/INNO/repos/video_process/live555_rtsp_live_v4l2/test_buf.h264","wa+");
    if(h264_fp_buf==NULL)
    {
      printf("buf 文件创建失败!\n");
      exit(1);
    }
    //   FILE * h264_fp_pipe=fopen("/home/demo/INNO/repos/video_process/live555_rtsp_live_v4l2/test_pipe.h264","wa+");
    // if(h264_fp_pipe==NULL)
    // {
    // 	printf("pipe 文件创建失败!\n");
    // 	exit(1);
    // }

    mkfifo(inputFileName, 0777);
    h264_fp = fopen(inputFileName, "wa+");
  //   Camera.Init();
  //   if(0 == fork())
  //   {
  //   Camera.pipe_fd = fopen(inputFileName, "wa+");
  //     while(1)
  //     {
  //       Camera.getnextframe();
  //     }
  // }
  }  

  pthread_t thread;
	
	/*绑定将要捕获的信号*/
	signal(SIGINT,exit_sighandler);
	signal(SIGSEGV,exit_sighandler);
	signal(SIGPIPE,SIG_IGN);
	
	/*1. 初始化摄像头*/
	if(UVCvideoInit()!=0)
	{
		printf("摄像头数据采集客户端:初始化摄像头失败!\n");
		exit(1);
	}
	
	/*2. 初始化编码器*/
	X264_init_encoder(Image_Width,Image_Height);
  pthread_create(&thread,NULL,pthread_video_Data_Handler,NULL);
	
	//设置线程的分离属性
	pthread_detach(thread);

  Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
  rtpGroupsock.multicastSendOnly(); // we're a SSM source
  Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
  rtcpGroupsock.multicastSendOnly(); // we're a SSM source

  // Create a 'H264 Video RTP' sink from the RTP 'groupsock':
  OutPacketBuffer::maxSize = 100000;
  videoSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case
  RTCPInstance* rtcp
  = RTCPInstance::createNew(*env, &rtcpGroupsock,
			    estimatedSessionBandwidth, CNAME,
			    videoSink, NULL /* we're a server */,
			    True /* we're a SSM source */);
  // Note: This starts RTCP running automatically

  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554);
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }
  ServerMediaSession* sms
    = ServerMediaSession::createNew(*env, "testStream", inputFileName,
		   "Session streamed by \"testH264VideoStreamer\"",
					   True /*SSM*/);
  sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
  rtspServer->addServerMediaSession(sms);
  // announceURL(rtspServer, sms);

    char* url = rtspServer->rtspURL(sms);
  *env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;  


  // Start the streaming:
  *env << "Beginning streaming...\n";
  play();

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void afterPlaying(void* /*clientData*/) {
  *env << "...done reading from file\n";
  videoSink->stopPlaying();
  Medium::close(videoSource);
  if(useStream){
    // Camera.Destory();
  }
  // Note that this also closes the input file that this source read from.

  // Start playing once again:
  play();
}

void play() {
  // Open the input file as a 'byte-stream file source':
  ByteStreamFileSource* fileSource
    = ByteStreamFileSource::createNew(*env, inputFileName);
  if (fileSource == NULL) {
    *env << "Unable to open file \"" << inputFileName
         << "\" as a byte-stream file source\n";
    exit(1);
  }

  FramedSource* videoES = fileSource;

  // Create a framer for the Video Elementary Stream:
  videoSource = H264VideoStreamFramer::createNew(*env, videoES);

  // Finally, start playing:
  *env << "Beginning to read from file...\n";
  videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

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
// #include "inc/x264.h"


// typedef struct
// {
// 	x264_param_t *param;
// 	x264_t *handle;
// 	x264_picture_t *picture; //说明一个视频序列中每帧特点
// 	x264_nal_t *nal;
// }Encoder;

// Encoder en;
 
/*摄像头相关的全局变量声明区域*/
#define UVC_VIDEO_DEVICE "/dev/video2"  /*UVC摄像头设备节点*/

int uvc_video_fd; /*存放摄像头设备节点的文件描述符*/
unsigned char *video_memaddr_buffer[4]; /*存放的是摄像头映射出来的缓冲区首地址*/
int Image_Width;  /*图像的宽度*/
int Image_Height; /*图像的高度*/
 
 x264_encoder *encoder_;
/*X264编码器相关的全局变量声明区域*/
unsigned char *h264_buf=NULL;


 
 

/*
函数功能: 处理退出的信号
*/
void exit_sighandler(int sig)
{
	/*关闭视频文件*/
	// fclose(h264_fp);
	
	//关闭摄像头
	close(uvc_video_fd);
	
	//释放缓冲区
	free(h264_buf);
		
	//退出进程
	exit(1);
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
	int encode_len = 0 ;
	while(1)
	{
		/*1. 等待摄像头采集数据*/
		poll(&fds,1,-1); 
 
		/*2. 取出一帧数据: 从采集队列里面取出一个缓冲区*/
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;   /*视频捕获设备*/
		ioctl(uvc_video_fd,VIDIOC_DQBUF,&buff_info); /*从采集队列取出缓冲区*/
		index=buff_info.index;
		printf("llb . buff_info.bytesused = %d \t",buff_info.bytesused);
		//printf("采集数据的缓冲区的编号:%d\n",index);
 
		/*3. 处理数据: 进行H264编码*/
		//video_memaddr_buffer[index]; /*当前存放数据的缓冲区地址*/
		
		/*编码一帧数据*/
		encode_len = encoder_->encode_frame(video_memaddr_buffer[index]);
		printf("h264_encode_length=%d\n",encode_len);
		/*4. 将缓冲区再次放入采集队列*/
		buff_info.memory=V4L2_MEMORY_MMAP; 	/*支持mmap内存映射*/
		buff_info.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; /*视频捕获设备*/
		buff_info.index=index; /*缓冲区的节点编号*/
		ioctl(uvc_video_fd,VIDIOC_QBUF,&buff_info); /*根据节点编号将缓冲区放入队列*/
	}
}
 
 
int main(int argc,char **argv)
{
	
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
	// X264_init_encoder(Image_Width,Image_Height);
	encoder_ = new x264_encoder(&my_encoder,Image_Width,Image_Height);

	if(argc==2)
	{
		if(std::atoi(argv[1] ) == 1){
			printf("save file in ~ folder");
			encoder_->file_test = true;
		}
	}
	
	/*3.创建存放视频的文件*/
	// h264_fp=fopen(argv[1],"wa+");
	// if(h264_fp==NULL)
	// {
	// 	printf("文件创建失败!\n");
	// 	exit(1);
	// }
		
	/*4. 创建线程采集摄像头数据并编码*/
	pthread_create(&thread,NULL,pthread_video_Data_Handler,NULL);
	
	//设置线程的分离属性
	pthread_detach(thread);
	
	while(1)
	{
		
	}
}

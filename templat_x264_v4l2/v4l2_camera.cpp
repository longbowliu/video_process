//
//  Created by Huang Victor on 2021.12.02
//

/*
    This program deal with the v4l2 device on linxu in the following steps:
    init_device
        1. open device
        2. check device function
        3. set device parameters
    allocate_buffer
        4. request buffer
        5. map buffer to user memory space
        6. queue buffer
    start_stream
        7. start stream
    capture
        8. dequeue buffer
        9. process image
        10. requeue buffer
    stop_stream
        11. stop stream
    uninit_device
        12. close device
*/

#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <asm/types.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "v4l2_camera.h"
#include "x264_encoder.h"

using namespace std;

v4l2Camera::v4l2Camera(string device_adress, int width_requested, int height_requested){
    // Initialize Camera
    device = device_adress;
    width = width_requested;
    height = height_requested;
    printf("Device set to: %s\n", device.c_str());
}

v4l2Camera::v4l2Camera() {}

v4l2Camera::~v4l2Camera(){
    stop_stream();
    uninit_device();
}
// x264_encoder *encoder_;
void v4l2Camera::init_device(){
    // 1. Open device
    char* dev = &device[0u];
    if ((fd = open(dev, O_RDWR)) == -1) {
        perror("Connection failed");
        exit(1);
    }

    // 2. Check device function
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("Capabilities Query failed");
        exit(1);
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        perror("Single-planar video capture");
        exit(1);
    }

    // 3. Set device parameters
    CLEAR(format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = v4l2Camera::width;
    format.fmt.pix.height = v4l2Camera::height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field=V4L2_FIELD_ANY; /*系统自动设置: 帧属性*/
    // encoder_ = new x264_encoder(v4l2Camera::width,v4l2Camera::height);
    // Request the set format
    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        perror("Requested format failed");
        exit(1);
    }
}

void v4l2Camera::allocate_buffer(){
    // 4. Request buffer
    struct v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.count = 8;
    bufrequest.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0) {
        perror("Buffer request failed");
        exit(1);
    }

    // Check buffer size
    if (bufrequest.count < 2){
        perror("Insufficient buffer memory!");
        exit(1);
    }

    printf("buffer size allocated %d\n", bufrequest.count);

    // 5. Map buffer to user memory space
    buffers_ = std::vector<Buffer>(bufrequest.count);

    for (auto i = 0u; i < bufrequest.count; ++i)
    {
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        ioctl(fd, VIDIOC_QUERYBUF, &buf);
        
        buffers_[i].index = buf.index;
        buffers_[i].length = buf.length;
        buffers_[i].start = 
                static_cast<unsigned char *>(
                mmap(
                    NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset));
        
        if (buffers_[i].start == MAP_FAILED) {
            perror("Memory mapping failed");
            exit(1);
        }
    }

    // 6. Queue buffers
    for (auto const & buffer:buffers_)
    {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer.index;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("Buffer incoming queue");
            exit(1);
        }
    }
}

void v4l2Camera::start_stream(){
    
    // 7. Start stream
    unsigned type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Streaming");
        exit(1);
    }
}

void v4l2Camera::capture( )
{
    // 8. Dequeue buffer
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("Buffer outgoing queue failed");
        exit(1);
    }
    // int encode_len = encoder_->encode_frame(buffers_[buf.index].start);
    // 9. Process image
    auto const & buffer = buffers_[buf.index];
    // printf("length: %d\n",buffer.length);
    // cv::Mat raw_input(format.fmt.pix.height, format.fmt.pix.width, CV_8UC2, buffer.start);
    // v4l2Camera::raw_input = raw_input;
    unsigned char * test =new unsigned char[buffer.length];
    v4l2Camera::r_ts.prt = test;
    v4l2Camera::r_ts.t =  buf.timestamp;
     v4l2Camera::r_ts.length = buffer.length;
    // memset(test,0,buffer.length);
    memcpy(test, buffers_[buf.index].start,buffer.length);
    printf("hi 123: %d",v4l2Camera::r_ts.prt);
    // int encode_len = encoder_->encode_frame( v4l2Camera::r_ts.prt );
    v4l2Camera::buffer_ts = buf.timestamp;
    
    // 10. Requeue buffer
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        perror("Buffer incoming queue");
        exit(1);
    }

    return;
}


void v4l2Camera::stop_stream()
{
    // 11. Stop stream
    unsigned type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("Streaming");
        exit(1);
    }

    // Return buffers
    for (auto const & buffer : buffers_) {
        munmap(buffer.start, buffer.length);
    }

    buffers_.clear();

    auto req = v4l2_requestbuffers{};

    // Free all buffers
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);    
}

void v4l2Camera::uninit_device() {
    //12. Close device
    close(fd);
}

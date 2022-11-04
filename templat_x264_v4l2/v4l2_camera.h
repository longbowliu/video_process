//
//  Created by Huang Victor on 2021.12.02
//

#ifndef V4L2_CAMERA_H
#define V4L2_CAMERA_H

#include <string>
#include <cstring>
#include <linux/videodev2.h>
#include <vector>
#include<queue>
#include <mutex>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
struct raw_ts{
    unsigned char * prt;
    size_t length;
    timeval t;
};



class v4l2Camera{
    public:
        int width;
        int height;
        timeval buffer_ts;
        raw_ts r_ts;
        std::mutex q_lock;

        v4l2Camera();
        v4l2Camera(std::string device_adress = "/dev/video2", int width = 1920, int height = 1080);
        ~v4l2Camera();
        void init_device();
        void allocate_buffer();
        void start_stream();
        void capture();
        void stop_stream();
        void uninit_device();
        

    private:
        int fd;
        std::string device;
        struct v4l2_capability cap;
        struct v4l2_format format;
        struct v4l2_buffer buf;

        struct Buffer
        {
            unsigned index;
            unsigned char * start;
            size_t length;
        };

        std::vector<Buffer> buffers_;

};

#endif

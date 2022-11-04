//
//  Created by Huang Victor on 2021.12.02
//

#include <ros/ros.h>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <opencv2/opencv.hpp>
#include "v4l2_camera.h"
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
// #include <sensor_msgs/CompressedImage>
#include <camera_info_manager/camera_info_manager.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <time.h>
#include <dirent.h>
#include <boost/filesystem.hpp>

#include <chrono>
#include <thread>
#include <mutex>

struct mat_ts
{
    cv::Mat mat;
    timeval ts;
};


// capture image and timestamp to queue
void capture_image(v4l2Camera &camera, std::queue<mat_ts> &mat_ts_queue)
{
    while (true)
    {
        // Get the image as cv Mat type
        // auto start = std::chrono::system_clock::now();
        camera.capture();
        
        cv::Mat cur_image = camera.raw_input.clone();
        timeval cur_ts = camera.buffer_ts;
        mat_ts_queue.push({cur_image,cur_ts});

        // auto end2 = std::chrono::system_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end2-start).count();
        // std::cout<<"capture image time:"<< duration<<std::endl;
    }
        
}


// write image to disk
void save_image_disk(std::queue<mat_ts> &mat_ts_queue, std::string &record_path, std::string &camera_name)
{
    
    while (true)
    {
        
        if (!mat_ts_queue.empty())
        {
            // auto start = std::chrono::system_clock::now();

            cv::Mat image = mat_ts_queue.front().mat;
            timeval image_ts = mat_ts_queue.front().ts;
            
            
            // cvt to bgr for opencv
            cv::cvtColor(image, image, cv::COLOR_YUV2BGR_YUYV);
                
            // create folder path
            // record_path/202112030808/camera0/xxxxxxxxxxxx.jpg
            long timestamp = image_ts.tv_sec * 1e3+ image_ts.tv_usec/1e3; //ms
            time_t rawtime = image_ts.tv_sec; 
            struct tm *timeinfo = localtime(&rawtime);
            char str_time[100];
            sprintf(str_time, "%04d%02d%02d%02d%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min);
            std::string sub_folder(str_time);
            std::string path = record_path + "/" + sub_folder + "/" + camera_name + "/" ;
            boost::filesystem::create_directories(path);
            
            // write image to disk
            cv::imwrite(path+std::to_string(timestamp)+"_"+camera_name+".jpg",image);

            mat_ts_queue.pop();

            // auto end2 = std::chrono::system_clock::now();
            // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end2-start).count();
            // std::cout<<"save image time:"<< duration<<std::endl;
        }
        
       
    }
    
}

int main(int argc, char* argv[])
{    
    // Initialize node
    ros::init(argc, argv, "v4l2_camera_node");
    ros::NodeHandle nh("v4l2_camera");
    ros::NodeHandle nh_private("~");

    // Ros parameter handle
    std::string device, camera_info_url, encoding, record_path;
    int width, height;
    bool save_image, publish_info;
    nh_private.param("device", device, std::string("/dev/video0"));
    nh_private.param("width", width, 1920);
    nh_private.param("height", height, 1280);
    nh_private.param("camera_info_url", camera_info_url, std::string(""));
    nh_private.param("image_encoding", encoding, std::string("rgb8"));
    nh_private.param("save_image", save_image, true);
    nh_private.param("publish_info", publish_info, false);
    nh_private.param("record_path", record_path, std::string("/tmp/record"));

    std::string camera_name = std::string("camera") + device[10];

    // Initialize camera info haldler
    // std::shared_ptr<camera_info_manager::CameraInfoManager> cinfo;
    // cinfo.reset(new camera_info_manager::CameraInfoManager(nh, camera_name + "/camera_info", camera_info_url));
    
    // Initizalize camera
    v4l2Camera camera = v4l2Camera(device, width, height);
    camera.init_device();
    camera.allocate_buffer();
    camera.start_stream();


    // queue the cv mat and ts for multi thread
    std::queue<mat_ts> mat_ts_queue;


    while (ros::ok())
    {
        // capture thread
        std::thread th1(save_image_disk,std::ref(mat_ts_queue),std::ref(record_path),std::ref(camera_name));
        std::thread th0(capture_image, std::ref(camera),std::ref(mat_ts_queue));
        th0.join();
        th1.join();
        ros::spinOnce();
    
    }
    
    camera.stop_stream();
    camera.uninit_device();
    return 0;

}



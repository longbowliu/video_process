#include "v4l2_camera.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <thread>
#include<queue>
#include "x264_encoder.h"


 x264_encoder *encoder_;
//  Encoder my_encoder;
int stop=0;

std::queue<raw_ts>  q; 
std::queue<std::string> str_q;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

void capture_image(v4l2Camera &camera, std::queue<raw_ts>  &que2 ,std::queue<std::string> & str_q1 ,std::mutex &q_l )
{
    while (true)
    {
        // Get the image as cv Mat type
        // auto start = std::chrono::system_clock::now();
        camera.capture();
        raw_ts t ={};
        t.prt = camera.r_ts.prt;
        printf("hi456, %d",camera.r_ts.prt);
       
        t.length = camera.r_ts.length;
        t.t = camera.buffer_ts;
        // std::unique_lock<std::mutex> lock(q_l);
        que2.push( t );
        // str_q1.push("test");
        // delete[] camera.r_ts.prt;
        printf(" pushed =%d  \t",que2.size());

        
        // cv::Mat cur_image = camera.raw_input.clone();
        // timeval cur_ts = camera.buffer_ts;
        // mat_ts_queue.push({cur_image,cur_ts});

        // auto end2 = std::chrono::system_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end2-start).count();
        // std::cout<<"capture image time:"<< duration<<std::endl;
    }
        
}

void save_image_disk(std::queue<raw_ts> &mat_ts_queue, std::string &record_path, std::string &camera_name,std::mutex &q_l)
{
    FILE *h264_fp = fopen("/home/demo/test_buf_recorder123.h264","wa+");
    
    while (true)
    {
        // std::unique_lock<std::mutex> lock(q_l);
        if (!mat_ts_queue.empty())
        {
       
            // auto start = std::chrono::system_clock::now();

            // cv::Mat image = mat_ts_queue.front().mat;
            timeval image_ts = mat_ts_queue.front().t;
            
            
            
            // // cvt to bgr for opencv
            // cv::cvtColor(image, image, cv::COLOR_YUV2BGR_YUYV);
                
            // create folder path
            // record_path/202112030808/camera0/xxxxxxxxxxxx.jpg
            long timestamp = image_ts.tv_sec * 1e3+ image_ts.tv_usec/1e3; //ms
            time_t rawtime = image_ts.tv_sec; 
            struct tm *timeinfo = localtime(&rawtime);
            char str_time[100];
            sprintf(str_time, "%04d%02d%02d%02d%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min);
            std::string sub_folder(str_time);
            std::string path = record_path + "/" + sub_folder + "/" + "camera_name" + "/" ;
            // boost::filesystem::create_directories(path);
             int encode_len = encoder_->encode_frame( mat_ts_queue.front().prt );
            // int len = enc->encode_frame(mat_ts_queue.front().prt);
            fwrite(mat_ts_queue.front().prt, mat_ts_queue.front().length,1,h264_fp);
            // fwrite(enc->encoded_frame, len,1,h264_fp);
            // // write image to disk
            // cv::imwrite(path+std::to_string(timestamp)+"_"+camera_name+".jpg",image);

            mat_ts_queue.pop();
             printf("after pop , %d\n",mat_ts_queue.size());
            // auto end2 = std::chrono::system_clock::now();
            // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end2-start).count();
            // std::cout<<"save image time:"<< duration<<std::endl;
        }
        
       
    }
    
}
/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	v4l2Camera camera("/dev/video0",640,320);
    // v4l2Camera camera("/dev/video2",1920,1080);
     camera.init_device();
    camera.allocate_buffer();
    camera.start_stream();
    printf("camera.width=%d",camera.width);
    encoder_ = new x264_encoder(camera.width,camera.height);
	std::thread th0(capture_image, std::ref(camera), std::ref(q),std::ref(str_q),std::ref(camera.q_lock));
        std::string a = "";
        std::string b = "";
         std::thread th1(save_image_disk,std::ref(q),std::ref(a),std::ref(b),std::ref(camera.q_lock));
        th0.join();
          th1.join();
    // camera.capture();
	// if (videoCapture == NULL)
	// {	
	// }
	// else
	// {
		
	// }
	
	return 0;
}

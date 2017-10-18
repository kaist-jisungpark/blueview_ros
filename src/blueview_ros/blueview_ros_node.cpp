#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <blueview_ros/Sonar.h>

// #include <std_msgs/String.h>
#include <iostream>
#include <fstream>
#include <sstream>

// #include <sensor_msgs/Image.h>
// #include <sensor_msgs/image_encodings.h>

#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>


#include <boost/filesystem.hpp>

using std::cout;
using std::endl;

// Create a Sonar
Sonar sonar;

void MinRangeCallback(const std_msgs::Float32::ConstPtr& msg)
{
    sonar.set_min_range(msg->data);
}

void MaxRangeCallback(const std_msgs::Float32::ConstPtr& msg)
{
    sonar.set_max_range(msg->data);
}

//// Open camera for recording
cv::VideoWriter record_;
bool record_initialized_ = false;
std::string video_filename_ = "";
bool logging_enabled_ = false;
cv_bridge::CvImagePtr cv_img_ptr_;
bool cv_ptr_valid_ = false;

std::string notes_filename_ = "";
std::fstream notes_file_;
std::string log_filename_ = "";
std::fstream log_file_;
int log_frame_num_ = 0;

void videoCallback(const sensor_msgs::ImageConstPtr &msg)
{
    cv_img_ptr_ = cv_bridge::toCvCopy(msg, "bgr8");
    cv_ptr_valid_ = true;
}

void EnableSonarLoggingCallback(const std_msgs::Bool::ConstPtr& msg)
{
    logging_enabled_ = msg->data;

    sonar.SonarLogEnable(msg->data);
    if (logging_enabled_) {
        // Generate the avi file name
        video_filename_ = sonar.current_sonar_file() + ".avi";
        log_filename_ = sonar.current_sonar_file() + ".txt";
        notes_filename_ = sonar.current_sonar_file() + ".notes";
        log_frame_num_ = 0;
    } else {
        log_file_.close();
        notes_file_.close();
    }

    record_initialized_ = false;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "bluview_ros_node");
    ros::NodeHandle n;
    ros::NodeHandle nh("~");

    //Publish opencv image of sonar
    image_transport::ImageTransport it(n);
    image_transport::Publisher image_pub = it.advertise("sonar_image", 1);

    ///////////////////////////////////////////////////
    // Acquire params from paramserver
    ///////////////////////////////////////////////////
    // Grab minimum distance
    double min_dist=-2;
    nh.getParam("min_dist", min_dist);

    // Grab maximim distance
    double max_dist;
    nh.getParam("max_dist", max_dist);

    // Set sonar range
    sonar.set_range(min_dist, max_dist);

    // Grab mode (image or range)
    std::string mode;
    nh.getParam("mode", mode);
    if (mode == "range") {
        sonar.set_data_mode(Sonar::range);
    } else {
        sonar.set_data_mode(Sonar::image);
    }

    // Grab color map filename
    std::string color_map;
    nh.getParam("color_map", color_map);
    sonar.set_color_map(color_map);

    //  // Grab sonar save directory
    // //  std::string save_directory;
    // //  node.get_param("~save_directory", save_directory);
    // //  sonar.set_save_directory(save_directory);
    //
    //
    // //  // Create the save_directory if it doesn't exist
    // //  boost::filesystem::path dir(save_directory);
    // //  if( !(boost::filesystem::exists(dir))) {
    // //       if(boost::filesystem::create_directory(dir)) {
    // //            std::cout << "Created new sonar save directory: " <<
    // //                 save_directory << endl;
    // //       } else {
    // //            cout << "Failed to create directory: " << save_directory << endl;
    // //       }
    // //  }
    //
    // Determine if a live "net" sonar will be used or if we are reading
    // from a file
    std::string net_or_file;
    // Grab sonar file name
    std::string sonar_file;
    // Grab suggested ip address
    std::string ip_addr;
    nh.getParam("net_or_file", net_or_file);

    if (net_or_file == "net") {
        nh.getParam("ip_addr", ip_addr);
        sonar.set_mode(Sonar::net);
        sonar.set_ip_addr(ip_addr);
    } else {
        nh.getParam("sonar_file", sonar_file);
        sonar.set_mode(Sonar::sonar_file);
        sonar.set_input_son_filename(sonar_file);
    }

    // Initialize the sonar
    sonar.init();

    //Subscribe to range commands
    ros::Subscriber min_range_sub = nh.subscribe("sonar_min_range", 1,
                                                 MinRangeCallback);
    ros::Subscriber max_range_sub = nh.subscribe("sonar_max_range", 1,
                                                 MaxRangeCallback);

    cout << "min_dist: " << min_dist << endl;
    cout << "max_dist: " << max_dist << endl;
    cout << "color_map: " << color_map << endl;
    cout << "sonar_file: " << sonar_file << endl;
    //  ros::Subscriber enable_log_sub = nh_.subscribe("sonar_enable_log", 1,
    //                                                EnableSonarLoggingCallback);
    //
    //

     // cv bridge static settings:
     cv_bridge::CvImage cvi;
     cvi.header.frame_id = "image";
     cvi.encoding = "bgra8"; // sonar image is four channels

     // Image sensor message
     sensor_msgs::Image msg;
    // //
    // //  cv::Mat temp;
    // //  sonar.getNextSonarImage(temp);
    // //
    // //  //cv::VideoWriter record_;
    // //  //record_.open("/home/videoray/sonar_log/video.avi", CV_FOURCC('D','I','V','X'), 10, temp.size(), true);
    // //
    ros::Time ros_time;

    while (ros::ok())
    {
        cv::Mat img;
        int status = sonar.getNextSonarImage(img);
        //
        // // Handle video logging
        // if (logging_enabled_ && cv_ptr_valid_) {
        //      if (!record_initialized_) {
        //           //record_.open(video_filename_,CV_FOURCC('M','J','P','G'),30,cv_img_ptr_->image.size(), true);
        //           record_.open(video_filename_,CV_FOURCC('D','I','V','X'),15,cv_img_ptr_->image.size(), true);
        //           record_initialized_ = true;
        //           log_file_.open(log_filename_.c_str(), std::ios::out);
        //           if (notes_file_.is_open()) {
        //                notes_file_.close();
        //           }
        //           notes_file_.open(notes_filename_.c_str(), std::ios::out);
        //      }
        //      record_ << cv_img_ptr_->image;
        //
        //      std::ostringstream frame_num_ss, time_sec_ss, time_nsec_ss;
        //      frame_num_ss << log_frame_num_++;
        //      ros_time = ros::Time::now();
        //      time_sec_ss << ros_time.sec;
        //      time_nsec_ss << ros_time.nsec;
        //
        //      // frame number, time (sec), time (nsec)
        //      log_file_ << frame_num_ss.str() << "," << time_sec_ss.str()
        //                << "," << time_nsec_ss.str() << endl;
        // }

        //cap_ >> video;
        //record_ << video;

        if (status == Sonar::Success) {
            try {
                // convert OpenCV image to ROS message
               cvi.header.stamp = ros::Time::now();
               cvi.image = img;
               cvi.toImageMsg(msg);

               // Publish the image
               image_pub.publish(msg);

            } catch (cv_bridge::Exception& e) {
                ROS_ERROR("cv_bridge exception: %s", e.what());
                return -1;
            }

            //cv::imshow("sonar", img);
            //cv::imshow("video", video);
            cv::waitKey(33);
        }


        ros::spinOnce();
    }

    return 0;
}

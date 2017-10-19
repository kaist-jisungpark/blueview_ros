#include <blueview_ros/Sonar.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
//#include <vector>
#define ENABLE_SONAR 1

using std::cout;
using std::endl;

Sonar::Sonar()
     : initialized_(false), fn_(""), ip_addr_(""), logging_(false),
       mode_(Sonar::net), data_mode_(Sonar::image), min_range_(0),
       max_range_(40), color_map_(""), save_directory_("./")
{
}

Sonar::~Sonar()
{
#if ENABLE_SONAR == 1
     if (son_) {
          BVTSonar_Destroy(son_);
     }

     // Close logging file
     this->SonarLogEnable(false);
#endif
}

Sonar::Status_t Sonar::init()
{
#if ENABLE_SONAR == 1
     logging_ = false;
     cur_ping_ = 0;

     son_ = BVTSonar_Create();
     if (son_ == NULL ) {
          printf("BVTSonar_Create: failed\n");
          return Sonar::Failure;
     }

     int ret;
     if (mode_ == Sonar::net) {
          /////////////////////////////////////////
          // Reading from physical sonar
          /////////////////////////////////////////

          // If the ip address string is set, try to manually connect
          // to sonar first.
          bool manual_sonar_found = false;
          if (ip_addr_ != "" && ip_addr_ != "0.0.0.0") {
               ret = BVTSonar_Open(son_, "NET", ip_addr_.c_str());
               if( ret != 0 ) {
                    printf("Couldn't find sonar at defined IP address: %s",
                           ip_addr_.c_str());
               } else {
                    manual_sonar_found = true;
               }
          }
          //
          // if (!manual_sonar_found) {
          //      //Create the discovery agent
          //      BVTSonarDiscoveryAgent agent = BVTSonarDiscoveryAgent_Create();
          //      if( agent == NULL ) {
          //           printf("BVTSonarDiscoverAgent_Create: failed\n");
          //           return Sonar::Failure;
          //      }
          //
          //      // Kick off the discovery process
          //      ret = BVTSonarDiscoveryAgent_Start(agent);
          //
          //      //Let the discovery process run for a short while (5 secs)
          //      cout << "Searching for available sonars..." << endl;
          //      sleep(5);
          //
          //      // See what we found
          //      int numSonars = 0;
          //      numSonars = BVTSonarDiscoveryAgent_GetSonarCount(agent);
          //
          //      char SonarIPAddress[20];
          //
          //      for(int i = 0; i < numSonars; i++) {
          //           ret = BVTSonarDiscoveryAgent_GetSonarInfo(agent, i, &SonarIPAddress[0], 20);
          //           printf("Found Sonar: %d, IP address: %s\n", i, SonarIPAddress);
          //      }
          //
          //      if(numSonars == 0) {
          //           printf("No Sonars Found\n");
          //           return Sonar::Failure;
          //      }
          //
          //      // Open the sonar
          //      //ret = BVTSonar_Open(son_, "NET", "192.168.1.45");
          //      ret = BVTSonar_Open(son_, "NET", SonarIPAddress);
          //      if( ret != 0 ) {
          //           printf("BVTSonar_Open: ret=%d\n", ret);
          //           return Sonar::Failure;
          //      }
          // }

     } else {
          /////////////////////////////////////////
          // Reading from sonar file
          /////////////////////////////////////////

          // Open the sonar
          ret = BVTSonar_Open(son_, "FILE", fn_.c_str());
          if (ret != 0 ) {
               printf("BVTSonar_Open: ret=%d\n", ret);
               return Sonar::Failure;
          }
     }

     // Make sure we have the right number of heads_
     heads_ = -1;
     heads_ = BVTSonar_GetHeadCount(son_);
     printf("BVTSonar_GetHeadCount: %d\n", heads_);

     // Get the first head
     head_ = NULL;
     ret = BVTSonar_GetHead(son_, 0, &head_);
     if (ret != 0 ) {
          // Some sonar heads start at 1
          ret = BVTSonar_GetHead(son_, 1, &head_);
          if (ret != 0) {
               printf( "BVTSonar_GetHead: ret=%d\n", ret) ;
               return Sonar::Failure;
          }
     }

     // Check the ping count
     pings_ = -1;
     pings_ = BVTHead_GetPingCount(head_);
     printf("BVTHead_GetPingCount: %d\n", pings_);

     // Set the range window
     this->set_range(min_range_, max_range_);

     initialized_ = true;

     return Sonar::Success;
#else
     return Sonar::Failure;
#endif

}

int Sonar::getNumPings()
{
     return pings_;
}

int Sonar::getCurrentPingNum()
{
     return cur_ping_;
}

void Sonar::setFrameNum(int num)
{
     cur_ping_ = num;
}

int Sonar::reset()
{
     cur_ping_ = 0;
     return 0;
}

Sonar::Status_t Sonar::SonarLogEnable(bool enable)
{
     // Whether enable is true or false, if we enter the function here,
     // we should properly close the current file if currently logging
     if (logging_ && son_logger_) {
          BVTSonar_Destroy(son_logger_);
     }

     if (!enable) {
          // If logging is disabled, exit.
          logging_ = false;
          return Sonar::Success;
     }

     son_logger_ = BVTSonar_Create();
     if (son_logger_ == NULL) {
          printf("BVTSonar_Create: failed\n");
          return Sonar::Failure;
     }

     // Create the sonar file
    //  cur_log_file_ = save_directory_ + "/" + syllo::get_time_string() + ".son";
     cur_log_file_ = save_directory_ + "/son.son";

     int ret = BVTSonar_CreateFile(son_logger_, cur_log_file_.c_str(),
                                   son_, "");

     if (ret != 0) {
          printf("BVTSonar_CreateFile: ret=%d\n", ret);
          return Sonar::Failure;
     }

     // Get the first head of the file output
     out_head_ = NULL ;
     ret = BVTSonar_GetHead(son_logger_, 0, &out_head_);
     if (ret != 0) {
          printf("BVTSonar_GetHead: ret=%d\n" ,ret);
          return Sonar::Failure;
     }

     logging_ = true;

     return Sonar::Success;
}

Sonar::Status_t Sonar::getNextSonarData()
{
     Status_t status = Sonar::Failure;
     if (mode_ == Sonar::net) {
          status = getSonarData(-1);
     } else if (cur_ping_ < pings_) {
          status = getSonarData(cur_ping_++);
     } else {
          status = Sonar::Failure;
     }
     return status;
}

Sonar::Status_t Sonar::getSonarData(int index)
{
#if ENABLE_SONAR == 1

     if (!initialized_)
     {
          cout << "Sonar wasn't initialized." << endl;
          return Sonar::Failure;
     }

     if(ping_)
        BVTPing_Destroy(ping_);

     int ret = BVTHead_GetPing(head_, index, &ping_);

     if(ret != 0) {
          printf("BVTHead_GetPing: ret=%d\n", ret);
          return Sonar::Failure;
     }

     // Logging is enabled, write to file
     if (logging_)
     {
          ret = BVTHead_PutPing(out_head_, ping_);
          if (ret != 0)
          {
               printf("BVTHead_PutPing: ret=%d\n", ret);
               return Sonar::Failure;
          }
     }

     return Sonar::Success;
#else
     return Sonar::Failure;
#endif
}

Sonar::Status_t Sonar::getSonarImage(cv::Mat &image)
{
    BVTMagImage img;
    BVTColorImage cimg;

    // Build a color mapper
    BVTColorMapper mapper = BVTColorMapper_Create();
    if (mapper == NULL)
    {
         printf("BVTColorMapper_Create: failed\n");
         return Sonar::Failure;
    }

    // Load the bone colormap
    int ret = BVTColorMapper_Load(mapper, color_map_.c_str());
    if(ret != 0)
    {
         if (color_map_ == "")
         {
              printf("Color map not set.\n");
         }
         printf("BVTColorMapper_Load: ret=%d\n", ret);
         return Sonar::Failure;
    }


    ret = BVTPing_GetImage(ping_, &img);
    //ret = BVTPing_GetImageXY(ping, &img);
    //ret = BVTPing_GetImageRTheta(ping, &img);
    if (ret != 0)
    {
         printf("BVTPing_GetImage: ret=%d\n", ret);
         return Sonar::Failure;
    }

    // Perform the colormapping
    ret = BVTColorMapper_MapImage(mapper, img, &cimg);
    if (ret != 0)
    {
         printf("BVTColorMapper_MapImage: ret=%d\n", ret);
         return Sonar::Failure;
    }

    height_ = BVTColorImage_GetHeight(cimg);
    width_ = BVTColorImage_GetWidth(cimg);

    cv::Mat imgcv(height_, width_, CV_8UC4, BVTColorImage_GetBits(cimg), width_*4);
    image = imgcv;

    BVTMagImage_Destroy(img);
    BVTColorMapper_Destroy(mapper);
    BVTColorImage_Destroy(cimg);
    return Sonar::Success;
}

Sonar::Status_t Sonar::getSonarScan(std::vector<double> &ranges)
{
    Status_t status = Sonar::Failure;
    if(cur_ping_> 0) //For the real BlueView is not going to work
    {
        status = Sonar::Success;

        BVTPing_GetRangeData( ping_, range_ );
        ranges.resize(range_.GetCount());
        for(int j=0; j<range_.GetCount(); j++)
            ranges[j] = range_.GetRangeValue(j);
    }

    return status;
}

int Sonar::width()
{
     return width_;
}

int Sonar::height()
{
     return height_;
}

void Sonar::set_mode(SonarMode_t mode)
{
     mode_ = mode;
}

void Sonar::set_data_mode(DataMode_t data_mode)
{
     data_mode_ = data_mode;
}

void Sonar::set_ip_addr(const std::string &ip_addr)
{
     ip_addr_ = ip_addr;
}

void Sonar::set_input_son_filename(const std::string &fn)
{
     fn_ = fn;
}

void Sonar::set_range(double min_range, double max_range)
{
     min_range_ = min_range;
     max_range_ = max_range;

     if (min_range_ < 0 || min_range_ > max_range_ ) {
          min_range = 0;
     }

     if (max_range_ < 0 || max_range_ <= min_range_+1) {
          max_range_ = min_range_ + 2;
     }

     BVTHead_SetRange(head_, min_range_, max_range_);
}

void Sonar::set_min_range(double min_range)
{
     this->set_range(min_range, max_range_);
}

void Sonar::set_max_range(double max_range)
{
     this->set_range(min_range_, max_range);
}

void Sonar::set_color_map(const std::string &color_map)
{
     color_map_ = color_map;
}

void Sonar::set_save_directory(const std::string &save_directory)
{
     save_directory_ = save_directory;
}

const std::string& Sonar::current_sonar_file()
{
     return cur_log_file_;
}

/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdio>
#include <time.h>
#include <pthread.h>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <sys/time.h>

#include <ros/ros.h>
#include <std_msgs/String.h>

#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/CompressedImage.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/PoseArray.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <pos_db.h>

//store subscribed value
static std::vector <geometry_msgs::PoseArray> car_position_array;
static std::vector <geometry_msgs::PoseArray> pedestrian_position_array;
//flag for comfirming whether updating position or not
static size_t car_num = 0;
static size_t pedestrian_num = 0;

//default server name and port to send data
static const std::string default_host_name = "db3.ertl.jp";
static constexpr int db_port = 5678;
static int sleep_msec = 500;		// period
static int use_current_time = 0;

//send to server class
static SendData sd;

//store own position and direction now.updated by position_getter
static std::vector <geometry_msgs::PoseStamped> ndt_position;
pthread_mutex_t pose_lock_;

static char mac_addr[20];

static std::string getTimeStamp(time_t sec, time_t nsec)
{
  char buf[30];
  int msec = static_cast<int>(nsec / (1000 * 1000));

  tm *t = localtime(&sec);
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d.%d",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec, msec);

  return std::string(static_cast<const char*>(buf));
}

static std::string pose_to_insert_statement(const geometry_msgs::Pose& pose, const std::string& timestamp, const char *name)
{
  std::ostringstream oss;
  constexpr int AREA = 7;

  oss << "INSERT INTO POS(id,x,y,z,area,or_x,or_y,or_z,or_w,type,tm) "
      << "VALUES("
      << "'" << name << ":" << mac_addr << "',"
      << std::fixed << std::setprecision(6) << pose.position.y << ","
      << std::fixed << std::setprecision(6) << pose.position.x << ","
      << std::fixed << std::setprecision(6) << pose.position.z << ","
      << AREA << ","
      << std::fixed << std::setprecision(6) << pose.orientation.y << ","
      << std::fixed << std::setprecision(6) << pose.orientation.x << ","
      << std::fixed << std::setprecision(6) << pose.orientation.z << ","
      << std::fixed << std::setprecision(6) << pose.orientation.w << ","
      << "0,"
      << "'" << timestamp << "'"
      << ");";

  return oss.str();
}

static std::string makeSendDataDetectedObj(const geometry_msgs::PoseArray& cp_array, const char *name)
{
  std::string timestamp;
  if(use_current_time) {
    ros::Time t = ros::Time::now();
    timestamp = getTimeStamp(t.sec, t.nsec);
  } else {
    timestamp = timestamp = getTimeStamp(cp_array.header.stamp.sec, cp_array.header.stamp.nsec);
  }

  std::string ret;
  for(const auto& pose : cp_array.poses){
    //create sql
    ret += pose_to_insert_statement(pose, timestamp, name);
    ret += "\n";
  }

  return ret;
}

//wrap SendData class
static void send_sql()
{
  int sql_num = car_num + pedestrian_num + ndt_position.size();
  std::cout << "sqlnum : " << sql_num << std::endl;

  //create header
  std::string value = make_header(2, sql_num);

std::cout << "ndt_num=" << ndt_position.size() << ", car_num=" << car_num << "(" << car_position_array.size() << ")" << ",pedestrian_num=" << pedestrian_num << "(" << pedestrian_position_array.size() << ")" << ", val=";

  //get data of car and pedestrian recognizing
  pthread_mutex_lock(&pose_lock_);
  if(car_num > 0){
    for(size_t i = 0; i < car_position_array.size(); i++) {
      value += makeSendDataDetectedObj(car_position_array[i], "car_pose");
    }
  }
  car_position_array.clear();
  car_num = 0;

  if(pedestrian_num > 0){
    for(size_t i = 0; i < pedestrian_position_array.size(); i++) {
      value += makeSendDataDetectedObj(pedestrian_position_array[i], "pedestrian_pose");
    }
  }
  pedestrian_position_array.clear();
  pedestrian_num = 0;


  // my_location
  for(size_t i = 0; i < ndt_position.size(); i++) {
    std::string timestamp;
    if(use_current_time) {
      ros::Time t = ros::Time::now();
      timestamp = getTimeStamp(t.sec, t.nsec);
    } else {
      timestamp = getTimeStamp(ndt_position[i].header.stamp.sec,ndt_position[i].header.stamp.nsec);
    }
    value += pose_to_insert_statement(ndt_position[i].pose, timestamp, "ndt_pose");
    value += "\n";
  }
  ndt_position.clear();
  pthread_mutex_unlock(&pose_lock_);

  std::cout << value << std::endl;

  std::string res;
  int ret = sd.Sender(value, res, sql_num);
  if (ret < 0) {
    std::cerr << "Failed: sd.Sender" << std::endl;
    return;
  }

  std::cout << "retrun message from DBserver : " << res << std::endl;

  return;
}

static void* intervalCall(void *unused)
{
  while(1){
    //If angle and position data is not updated from previous data send,
    //data is not sent
    if((car_num + pedestrian_num + ndt_position.size()) <= 0) {
      usleep(sleep_msec*1000);
      continue;
    }

    send_sql();
    usleep(sleep_msec*1000);
  }

  return nullptr;
}

static void car_locate_cb(const geometry_msgs::PoseArray& car_locate)
{
  if(car_locate.poses.size() > 0) {
    pthread_mutex_lock(&pose_lock_);
    car_num += car_locate.poses.size();
    car_position_array.push_back(car_locate);
    pthread_mutex_unlock(&pose_lock_);
  }
}

static void pedestrian_locate_cb(const geometry_msgs::PoseArray& pedestrian_locate)
{
  if(pedestrian_locate.poses.size() > 0) {
    pthread_mutex_lock(&pose_lock_);
    pedestrian_num += pedestrian_locate.poses.size();
    pedestrian_position_array.push_back(pedestrian_locate);
    pthread_mutex_unlock(&pose_lock_);
  }
}

static void ndt_pose_cb(const geometry_msgs::PoseStamped &pose)
{
  pthread_mutex_lock(&pose_lock_);
  ndt_position.push_back(pose);
  pthread_mutex_unlock(&pose_lock_);
}

int main(int argc, char **argv)
{
  ros::init(argc ,argv, "pos_uploader");
  std::cout << "pos_uploader" << std::endl;

  pose_lock_ = PTHREAD_MUTEX_INITIALIZER;

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }
  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
  ioctl(sock, SIOCGIFHWADDR, &ifr);
  close(sock);
  snprintf(mac_addr, sizeof(mac_addr), "%.2x%.2x%.2x%.2x%.2x%.2x",
         (unsigned char)ifr.ifr_hwaddr.sa_data[0],
         (unsigned char)ifr.ifr_hwaddr.sa_data[1],
         (unsigned char)ifr.ifr_hwaddr.sa_data[2],
         (unsigned char)ifr.ifr_hwaddr.sa_data[3],
         (unsigned char)ifr.ifr_hwaddr.sa_data[4],
         (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

  /**
   * NodeHandle is the main access point to communications with the ROS system.
   * The first NodeHandle constructed will fully initialize this node, and the last
   * NodeHandle destructed will close down the node.
   */
  ros::NodeHandle n;

  ros::Subscriber car_locate = n.subscribe("/car_pose", 1, car_locate_cb);
  ros::Subscriber pedestrian_locate = n.subscribe("/pedestrian_pose", 1, pedestrian_locate_cb);
  ros::Subscriber gnss_pose = n.subscribe("/ndt_pose", 1, ndt_pose_cb);


  //set server name and port
  std::string host_name = default_host_name;
  int port = db_port;
  if(argc >= 2) {
    for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "now", 3) == 0) use_current_time = 1;
    }
  }
  if(argc >= 3+use_current_time){
    host_name = argv[1];
    port = std::atoi(argv[2]);
  }

  sd = SendData(host_name, port);

  pthread_t th;
  if(pthread_create(&th, nullptr, intervalCall, nullptr)){
    printf("thread create error\n");
  }
  pthread_detach(th);

  ros::spin();
  return 0;
}

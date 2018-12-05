#ifndef DIAG_SUBSCRIBER_H_INCLUDED
#define DIAG_SUBSCRIBER_H_INCLUDED

// headers in STL
#include <algorithm>
#include <mutex>
#include <vector>

// headers in ROS
#include <ros/ros.h>

// headers in diag_msgs
#include <diag_msgs/diag_error.h>
#include <diag_msgs/diag_node_errors.h>

class DiagSubscriber {
public:
  DiagSubscriber(std::string target_node, int target_node_number);
  ~DiagSubscriber();
  diag_msgs::diag_node_errors getDiagNodeErrors();

private:
  std::mutex mtx_;
  std::vector<diag_msgs::diag_error> buffer_;
  ros::Subscriber diag_sub_;
  ros::NodeHandle nh_;
  void callback(diag_msgs::diag_error msg);
  const std::string target_node_;
  const int target_node_number_;
};

#endif // DIAG_SUBSCRIBER_H_INCLUDED
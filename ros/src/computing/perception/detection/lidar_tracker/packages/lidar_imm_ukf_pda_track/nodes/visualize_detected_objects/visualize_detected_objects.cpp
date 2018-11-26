/*
 * Copyright 2018 Autoware Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "visualize_detected_objects.h"

#include <visualization_msgs/MarkerArray.h>
#include <tf/transform_datatypes.h>
#include <cmath>

VisualizeDetectedObjects::VisualizeDetectedObjects() : vis_arrow_height_(0.5), vis_id_height_(1.5)
{
  ros::NodeHandle private_nh_("~");
  private_nh_.param<std::string>("pointcloud_frame", pointcloud_frame_, "velodyne");
  private_nh_.param<double>("ignore_velocity_thres", ignore_velocity_thres_, 0.1);
  private_nh_.param<double>("visualize_arrow_velocity_thres", visualize_arrow_velocity_thres_, 0.25);

  sub_object_array_ =
      node_handle_.subscribe("/detection/lidar_tracker/objects", 1, &VisualizeDetectedObjects::callBack, this);
  pub_arrow_ = node_handle_.advertise<visualization_msgs::MarkerArray>("/detection/lidar_tracker/arrow_markers", 10);
  pub_id_ = node_handle_.advertise<visualization_msgs::MarkerArray>("/detection/lidar_tracker/id_markes", 10);
}

void VisualizeDetectedObjects::callBack(const autoware_msgs::DetectedObjectArray& input)
{
  visMarkers(input);
}

void VisualizeDetectedObjects::visMarkers(const autoware_msgs::DetectedObjectArray& input)
{
  visualization_msgs::MarkerArray marker_ids, marker_arows;

  for (size_t i = 0; i < input.objects.size(); i++)
  {
    // pose_reliable == true if tracking state is stable
    // skip vizualizing if tracking state is unstable
    if (!input.objects[i].pose_reliable)
    {
      continue;
    }

    double velocity = input.objects[i].velocity.linear.x;

    tf::Quaternion q(input.objects[i].pose.orientation.x, input.objects[i].pose.orientation.y,
                     input.objects[i].pose.orientation.z, input.objects[i].pose.orientation.w);
    double roll, pitch, yaw;
    tf::Matrix3x3(q).getRPY(roll, pitch, yaw);

    // in the case motion model fit opposite direction
    if (velocity < -0.1)
    {
      velocity *= -1;
      yaw += M_PI;
      // normalize angle
      while (yaw > M_PI)
        yaw -= 2. * M_PI;
      while (yaw < -M_PI)
        yaw += 2. * M_PI;
    }

    visualization_msgs::Marker id;

    id.lifetime = ros::Duration(0.2);
    id.header.frame_id = pointcloud_frame_;
    id.header.stamp = input.header.stamp;
    id.ns = "id";
    id.action = visualization_msgs::Marker::ADD;
    id.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    // green
    id.color.g = 1.0f;
    id.color.a = 1.0;
    id.id = input.objects[i].id;

    // Set the pose of the marker.  This is a full 6DOF pose relative to the frame/time specified in the header
    id.pose.position.x = input.objects[i].pose.position.x;
    id.pose.position.y = input.objects[i].pose.position.y;
    id.pose.position.z = vis_id_height_;

    // convert from RPY to quartenion
    tf::Matrix3x3 obs_mat;
    obs_mat.setEulerYPR(yaw, 0, 0);  // yaw, pitch, roll
    tf::Quaternion q_tf;
    obs_mat.getRotation(q_tf);
    id.pose.orientation.x = q_tf.getX();
    id.pose.orientation.y = q_tf.getY();
    id.pose.orientation.z = q_tf.getZ();
    id.pose.orientation.w = q_tf.getW();

    id.scale.z = 1.0;

    if (abs(velocity) < ignore_velocity_thres_)
    {
      velocity = 0.0;
    }

    // convert unit m/s to km/h
    std::string s_velocity = std::to_string(velocity * 3.6);
    std::string modified_sv = s_velocity.substr(0, s_velocity.find(".") + 3);
    std::string text = "<" + std::to_string(input.objects[i].id) + "> " + modified_sv + " km/h";

    id.text = text;

    marker_ids.markers.push_back(id);

    visualization_msgs::Marker arrow;
    arrow.lifetime = ros::Duration(0.2);

    // visualize velocity arrow only if its status is Stable
    std::string label = input.objects[i].label;
    if (label == "None" || label == "Initialized" || label == "Lost" || label == "Static")
    {
      continue;
    }
    if (abs(velocity) < visualize_arrow_velocity_thres_)
    {
      continue;
    }

    arrow.header.frame_id = pointcloud_frame_;
    arrow.header.stamp = input.header.stamp;
    arrow.ns = "arrow";
    arrow.action = visualization_msgs::Marker::ADD;
    arrow.type = visualization_msgs::Marker::ARROW;
    // green
    arrow.color.g = 1.0f;
    arrow.color.a = 1.0;
    arrow.id = input.objects[i].id;

    // Set the pose of the marker.  This is a full 6DOF pose relative to the frame/time specified in the header
    arrow.pose.position.x = input.objects[i].pose.position.x;
    arrow.pose.position.y = input.objects[i].pose.position.y;
    arrow.pose.position.z = vis_arrow_height_;

    arrow.pose.orientation.x = q_tf.getX();
    arrow.pose.orientation.y = q_tf.getY();
    arrow.pose.orientation.z = q_tf.getZ();
    arrow.pose.orientation.w = q_tf.getW();

    // Set the scale of the arrow -- 1x1x1 here means 1m on a side
    arrow.scale.x = 3;
    arrow.scale.y = 0.1;
    arrow.scale.z = 0.1;

    marker_arows.markers.push_back(arrow);
  }  // end input.objects loop
  pub_id_.publish(marker_ids);
  pub_arrow_.publish(marker_arows);
}

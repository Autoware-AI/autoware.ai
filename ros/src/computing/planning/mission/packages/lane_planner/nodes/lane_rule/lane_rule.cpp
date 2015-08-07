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

#include <ros/ros.h>
#include <ros/console.h>

#include <waypoint_follower/lane.h>
#include <runtime_manager/ConfigLaneRule.h>

#include <vmap_utility.hpp>

static double config_acceleration = 1; // m/s^2
static int config_number_of_zeros = 1;

static ros::Publisher pub_traffic;
static ros::Publisher pub_red;
static ros::Publisher pub_green;

static VectorMap vmap_all;
static VectorMap vmap_left_lane;

static int sub_vmap_queue_size;
static int sub_waypoint_queue_size;
static int sub_config_queue_size;
static int pub_waypoint_queue_size;
static bool pub_waypoint_latch;

static void cache_left_lane()
{
	if (vmap_all.lanes.empty() || vmap_all.nodes.empty() || vmap_all.points.empty())
		return;

	for (const map_file::Lane& l : vmap_all.lanes) {
		if (l.lno != 1)	// leftmost lane
			continue;
		vmap_left_lane.lanes.push_back(l);
		for (const map_file::Node& n : vmap_all.nodes) {
			if (n.nid != l.bnid && n.nid != l.fnid)
				continue;
			vmap_left_lane.nodes.push_back(n);
			for (const map_file::PointClass& p : vmap_all.points) {
				if (p.pid != n.pid)
					continue;
				vmap_left_lane.points.push_back(p);
			}
		}
	}
}

static void cache_lane(const map_file::LaneArray& msg)
{
	vmap_all.lanes = msg.lanes;
	cache_left_lane();
}

static void cache_node(const map_file::NodeArray& msg)
{
	vmap_all.nodes = msg.nodes;
	cache_left_lane();
}

static void cache_point(const map_file::PointClassArray& msg)
{
	vmap_all.points = msg.point_classes;
	cache_left_lane();
}

static void cache_stopline(const map_file::StopLineArray& msg)
{
	vmap_all.stoplines = msg.stop_lines;
}

static void config_rule(const runtime_manager::ConfigLaneRule& msg)
{
	config_acceleration = msg.acceleration;
	config_number_of_zeros = msg.number_of_zeros;
}

static std::vector<map_file::PointClass> search_stopline_point(const waypoint_follower::lane& msg)
{
	std::vector<map_file::PointClass> stopline_points;

	map_file::PointClass start_point = vmap_find_nearest_point(vmap_left_lane,
								   msg.waypoints.front().pose.pose.position.x,
								   msg.waypoints.front().pose.pose.position.y);
	map_file::PointClass end_point = vmap_find_nearest_point(vmap_left_lane,
								 msg.waypoints.back().pose.pose.position.x,
								 msg.waypoints.back().pose.pose.position.y);

	map_file::PointClass point = start_point;
	map_file::Lane lane = vmap_find_lane(vmap_left_lane, point);
	if (lane.lnid < 0) {
		ROS_ERROR("no start lane");
		return stopline_points;
	}

	bool finish = false;
	for (size_t i = 0; i < std::numeric_limits<std::size_t>::max(); ++i) {
		for (const map_file::StopLine& s : vmap_all.stoplines) {
			if (s.linkid == lane.lnid)
				stopline_points.push_back(point);
		}

		if (finish)
			break;

		point = vmap_find_end_point(vmap_left_lane, lane);
		if (point.pid < 0) {
			ROS_ERROR("no end point");
			return stopline_points;
		}

		if (point.bx == end_point.bx && point.ly == end_point.ly) {
			finish = true;
			continue;
		}

		lane = vmap_find_next_lane(vmap_left_lane, lane);
		if (lane.lnid < 0) {
			ROS_ERROR("no next lane");
			return stopline_points;
		}

		point = vmap_find_start_point(vmap_left_lane, lane);
		if (point.pid < 0) {
			ROS_ERROR("no start point");
			return stopline_points;
		}
	}
	if (!finish)
		ROS_ERROR("miss finish");

	return stopline_points;
}

static std::vector<size_t> search_stopline_index(const waypoint_follower::lane& msg)
{
	std::vector<size_t> indexes;

	std::vector<map_file::PointClass> stopline_points = search_stopline_point(msg);
	for (const map_file::PointClass& p : stopline_points) {
		size_t i = 0;

		double min = hypot(p.bx - msg.waypoints[0].pose.pose.position.x,
				   p.ly - msg.waypoints[0].pose.pose.position.y);
		size_t index = i;
		for (const waypoint_follower::waypoint& w : msg.waypoints) {
			double distance = hypot(p.bx - w.pose.pose.position.x,
						p.ly - w.pose.pose.position.y);
			if (distance < min) {
				min = distance;
				index = i;
			}
			++i;
		}

		indexes.push_back(index);
	}

	return indexes;
}

static std::vector<double> compute_velocity(const waypoint_follower::lane& msg, double acceleration, int nzeros)
{
	std::vector<double> computations;

	std::vector<size_t> indexes = search_stopline_index(msg);
	if (indexes.empty()) {
		for (const waypoint_follower::waypoint& w : msg.waypoints)
			computations.push_back(w.twist.twist.linear.x);
		return computations;
	}

	// XXX no implementation of handling acceleration

	return computations;
}

static void create_traffic_waypoint(const waypoint_follower::lane& msg)
{
	std_msgs::Header header;
	header.stamp = ros::Time::now();
	header.frame_id = "/map";
	waypoint_follower::lane green;
	green.header = header;
	green.increment = 1;
	waypoint_follower::lane red;
	red.header = header;
	red.increment = 1;
	waypoint_follower::waypoint waypoint;
	waypoint.pose.header = header;
	waypoint.twist.header = header;
	waypoint.pose.pose.orientation.w = 1;

	std::vector<double> computations = compute_velocity(msg, config_acceleration, config_number_of_zeros);

	size_t loops = msg.waypoints.size();
	for (size_t i = 0; i < loops; ++i) {
		waypoint.pose.pose = msg.waypoints[i].pose.pose;
		waypoint.twist.twist = msg.waypoints[i].twist.twist;
		green.waypoints.push_back(waypoint);
		waypoint.twist.twist.linear.x = computations[i];
		red.waypoints.push_back(waypoint);
	}

	pub_traffic.publish(green);
	pub_green.publish(green);
	pub_red.publish(red);
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "lane_rule");

	ros::NodeHandle n;
	n.param<int>("/lane_navi/sub_vmap_queue_size", sub_vmap_queue_size, 1);
	n.param<int>("/lane_navi/sub_waypoint_queue_size", sub_waypoint_queue_size, 1);
	n.param<int>("/lane_navi/sub_config_queue_size", sub_config_queue_size, 1);
	n.param<int>("/lane_navi/pub_waypoint_queue_size", pub_waypoint_queue_size, 1);
	n.param<bool>("/lane_navi/pub_waypoint_latch", pub_waypoint_latch, true);

	ros::Subscriber sub_lane = n.subscribe("/vector_map_info/lane", sub_vmap_queue_size, cache_lane);
	ros::Subscriber sub_node = n.subscribe("/vector_map_info/node", sub_vmap_queue_size, cache_node);
	ros::Subscriber sub_point = n.subscribe("/vector_map_info/point_class", sub_vmap_queue_size, cache_point);
	ros::Subscriber sub_stopline = n.subscribe("/vector_map_info/stop_line", sub_vmap_queue_size, cache_stopline);
	ros::Rate rate(1);
	while (vmap_all.lanes.empty() || vmap_all.nodes.empty() || vmap_all.points.empty() ||
	       vmap_all.stoplines.empty()) {
		ros::spinOnce();
		rate.sleep();
	}
	ros::Subscriber sub_waypoint = n.subscribe("/lane_waypoint", sub_waypoint_queue_size, create_traffic_waypoint);
	ros::Subscriber sub_config = n.subscribe("/config/lane_rule", sub_config_queue_size, config_rule);

	pub_traffic = n.advertise<waypoint_follower::lane>("/traffic_waypoint", pub_waypoint_queue_size,
							   pub_waypoint_latch);
	pub_red = n.advertise<waypoint_follower::lane>("/red_waypoint", pub_waypoint_queue_size,
						       pub_waypoint_latch);
	pub_green = n.advertise<waypoint_follower::lane>("/green_waypoint", pub_waypoint_queue_size,
							 pub_waypoint_latch);

	ros::spin();

	return 0;
}

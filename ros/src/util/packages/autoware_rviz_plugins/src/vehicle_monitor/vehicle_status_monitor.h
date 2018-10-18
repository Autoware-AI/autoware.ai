#ifndef VHICLE_STATE_MONITOR_H_INCLUDED
#define VHICLE_STATE_MONITOR_H_INCLUDED

//headers in autoware
#include <autoware_msgs/VehicleStatus.h>
#include "overlay_utils.h"
#include "config.h"

// headers in ROS
#include <ros/package.h>
#include <ros/ros.h>
#include <std_msgs/String.h>

// headers in Qt
#include <QWidget>
#include <QPainter>
#include <QImage>
#include <QRect>

// headers in rviz
#include <rviz/message_filter_display.h>
#include <rviz/properties/float_property.h>
#include <rviz/properties/int_property.h>
//#include <rviz/properties/string_property.h>
#include <rviz/properties/enum_property.h>
#include <rviz/properties/status_property.h>
#include <rviz/uniform_string_stream.h>

//headers in boost
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>

//headers in STL
#include <math.h>
#include <algorithm>

namespace autoware_rviz_plugins {
    class VehicleStatusMonitor : public rviz::Display{
    Q_OBJECT
    public:
        VehicleStatusMonitor();
        virtual ~VehicleStatusMonitor();
    protected:
        virtual void onInitialize();
        virtual void reset();
        virtual void update(float wall_dt, float ros_dt);
        virtual void onEnable();
        virtual void onDisable();
    private:
        autoware_rviz_plugins::OverlayObject::Ptr overlay_;
        gear_status gear_status_;
        void processMessage(const autoware_msgs::VehicleStatus::ConstPtr& msg);
        void processControlMessage(const std_msgs::String::ConstPtr& msg);
        void draw_monitor_();
        boost::shared_ptr<rviz::RosTopicProperty> status_topic_property_;
        boost::shared_ptr<rviz::RosTopicProperty> ctrl_mode_topic_property_;
        boost::shared_ptr<rviz::IntProperty> top_property_;
        boost::shared_ptr<rviz::IntProperty> left_property_;
        boost::shared_ptr<rviz::IntProperty> width_property_;
        boost::shared_ptr<rviz::IntProperty> font_size_property_;
        boost::shared_ptr<rviz::IntProperty> max_accel_value_property_;
        boost::shared_ptr<rviz::IntProperty> min_accel_value_property_;
        boost::shared_ptr<rviz::IntProperty> max_brake_value_property_;
        boost::shared_ptr<rviz::IntProperty> min_brake_value_property_;
        boost::shared_ptr<rviz::FloatProperty> alpha_property_;
        boost::shared_ptr<rviz::EnumProperty> speed_unit_property_;
        boost::shared_ptr<rviz::EnumProperty> angle_unit_property_;
        boost::optional<autoware_msgs::VehicleStatus> last_status_data_;
        ros::Subscriber status_sub_;
        ros::Subscriber ctrl_mode_sub_;
        ros::NodeHandle nh_;
        boost::mutex mutex_;
        int monitor_top_,monitor_left_;
        float alpha_;
        int width_,height_;
        int speed_unit_,angle_unit_;
        int font_size_;
        double height_ratio_,width_ratio_;
        int max_accel_value_,min_accel_value_,max_brake_value_,min_brake_value_;
        std::string topic_name_;
        std::string ctrl_mode_topic_name_;
        std::string control_mode_;
        volatile bool draw_required_;
        //functions for draw
        void draw_gear_shift_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_left_lamp_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y, bool status);
        void draw_right_lamp_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y, bool status);
        void draw_operation_status_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_steering_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_steering_angle_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_steering_mode_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_speed_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_drive_mode_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_brake_bar_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_filled_brake_bar_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
        void draw_accel_bar_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y);
    protected Q_SLOTS:
        void update_ctrl_mode_topic_();
        void update_status_topic_();
        void update_top_();
        void update_left_();
        void update_alpha_();
        void update_speed_unit_();
        void update_angle_unit_();
        void update_width_();
        void update_font_size_();
        void update_max_accel_value_();
        void update_min_accel_value_();
        void update_max_brake_value_();
        void update_min_brake_value_();
    };
}
#endif //VHICLE_STATE_MONITOR_H_INCLUDED
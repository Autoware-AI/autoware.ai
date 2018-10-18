#include "vehicle_status_monitor.h"

namespace autoware_rviz_plugins{
    VehicleStatusMonitor::VehicleStatusMonitor() : rviz::Display(){
        gear_status_.load_params();
        control_mode_ = "";
        max_accel_value_property_ = boost::make_shared<rviz::IntProperty>("Max accel value", 4095, "Maximum accel value.",this, SLOT(update_max_accel_value_()));
        min_accel_value_property_ = boost::make_shared<rviz::IntProperty>("Min accel value",    0, "Minimum accel value.",this, SLOT(update_min_accel_value_()));
        max_brake_value_property_ = boost::make_shared<rviz::IntProperty>("Max brake value", 4095, "Maximum brake value.",this, SLOT(update_max_brake_value_()));
        min_brake_value_property_ = boost::make_shared<rviz::IntProperty>("Min brake value",    0, "Minimum brake value.",this, SLOT(update_min_brake_value_()));
        width_property_ = boost::make_shared<rviz::IntProperty>("Monitor width", DEFAULT_MONITOR_WIDTH, "Width of the monitor.",this, SLOT(update_width_()));
        left_property_ = boost::make_shared<rviz::IntProperty>("Left position", 0, "Left position of the monitor.",this, SLOT(update_left_()));
        top_property_ = boost::make_shared<rviz::IntProperty>("Top position", 0, "Top position of the monitor.",this, SLOT(update_top_()));
        font_size_property_ = boost::make_shared<rviz::IntProperty>("Font size", 20, "Top position of the monitor.",this, SLOT(update_font_size_()));
        alpha_property_ = boost::make_shared<rviz::FloatProperty>("Alpha", 0, "alpha of the monitor.",this, SLOT(update_alpha_()));
        speed_unit_property_  = boost::make_shared<rviz::EnumProperty>("Speed unit", "km/h" , "Unit of the speed",this, SLOT(update_speed_unit_()));
        speed_unit_property_->addOption("km/h", KM_PER_HOUR);
        speed_unit_property_->addOption("m/s", M_PER_SEC);
        angle_unit_property_ = boost::make_shared<rviz::EnumProperty>("Angle unit", "rad" , "Unit of the angle",this, SLOT(update_angle_unit_()));
        angle_unit_property_->addOption("rad", RAD);
        angle_unit_property_->addOption("deg", DEG);
        status_topic_property_ = boost::make_shared<rviz::RosTopicProperty>("VehicleStatus Topic", "",ros::message_traits::datatype<autoware_msgs::VehicleStatus>(),"autoware_msgs::VehicleStatus topic to subscribe to.",this, SLOT(update_status_topic_()));
        ctrl_mode_topic_property_ = boost::make_shared<rviz::RosTopicProperty>("CtrlCmd Topic", "",ros::message_traits::datatype<std_msgs::String>(),"std_msgs::String topic to subscribe to.",this, SLOT(update_ctrl_mode_topic_()));
    }

    VehicleStatusMonitor::~VehicleStatusMonitor(){
        //onDisable();
    }

    void VehicleStatusMonitor::onInitialize(){
        //overlay_ = boost::make_shared<OverlayObject>("VehicleStatusMonitor");
        //onEnable();
        update_top_();
        update_left_();
        update_alpha_();
        update_angle_unit_();
        update_speed_unit_();
        update_width_();
        update_font_size_();
        update_max_accel_value_();
        update_min_accel_value_();
        update_max_brake_value_();
        update_min_brake_value_();
        update_status_topic_();
        update_ctrl_mode_topic_();
        return;
    }

    void VehicleStatusMonitor::reset(){
        return;
    }

    void VehicleStatusMonitor::update(float wall_dt, float ros_dt){
        if (!isEnabled()){
            return;
        }
        if(draw_required_){
            draw_monitor_();
            draw_required_ = false;
        }
        return;
    }

    void VehicleStatusMonitor::update_font_size_(){
        boost::mutex::scoped_lock lock(mutex_);
        font_size_ = font_size_property_->getInt();
        return;
    }

    void VehicleStatusMonitor::update_speed_unit_(){
        boost::mutex::scoped_lock lock(mutex_);
        speed_unit_ = speed_unit_property_->getOptionInt();
        return;
    }

    void VehicleStatusMonitor::update_angle_unit_(){
        boost::mutex::scoped_lock lock(mutex_);
        angle_unit_ = angle_unit_property_->getOptionInt();
        return;
    }

    void VehicleStatusMonitor::update_width_(){
        boost::mutex::scoped_lock lock(mutex_);
        width_ = width_property_->getInt();
        height_ = (int)((double)width_*1.2);
        return;
    }

    void VehicleStatusMonitor::update_max_accel_value_(){
        boost::mutex::scoped_lock lock(mutex_);
        max_accel_value_ =  max_accel_value_property_->getInt();
        return;
    }

    void VehicleStatusMonitor::update_min_accel_value_(){
        boost::mutex::scoped_lock lock(mutex_);
        min_accel_value_ =  min_accel_value_property_->getInt();
        return;
    }

    void VehicleStatusMonitor::update_max_brake_value_(){
        boost::mutex::scoped_lock lock(mutex_);
        max_brake_value_ =  max_brake_value_property_->getInt();
        return;
    }

    void VehicleStatusMonitor::update_min_brake_value_(){
        boost::mutex::scoped_lock lock(mutex_);
        min_brake_value_ =  min_brake_value_property_->getInt();
        return;
    }

    void VehicleStatusMonitor::draw_monitor_(){
        boost::mutex::scoped_lock lock(mutex_);
        if(!last_status_data_)
        {
            return;
        }
        /*
        Functions to draw monitor
        */
        if(!overlay_)
        {
            static int count = 0;
            rviz::UniformStringStream ss;
            ss << "VehicleStatusMonitorObject" << count++;
            overlay_.reset(new OverlayObject(ss.str()));
            overlay_->show();
        }
        if(overlay_)
        {
            overlay_->setDimensions(width_,height_);
            overlay_->setPosition(monitor_left_,monitor_top_);
        }
        overlay_->updateTextureSize(width_,height_);
        ScopedPixelBuffer buffer = overlay_->getBuffer();
        QImage Hud = buffer.getQImage(*overlay_);
        for (unsigned int i = 0; i < overlay_->getTextureWidth(); i++) {
            for (unsigned int j = 0; j < overlay_->getTextureHeight(); j++) {
                Hud.setPixel(i, j, QColor(0,0,0,(int)(255*alpha_)).rgba());
            }
        }
        width_ratio_ = (double)width_ / (double)DEFAULT_MONITOR_WIDTH;
        height_ratio_ = (double)height_ / (double)DEFAULT_MONITOR_WIDTH;
        /*
        Setup QPainter
        */
        boost::shared_ptr<QPainter> painter = boost::make_shared<QPainter>(&Hud);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(QColor(0,255,255,(int)(255*alpha_)).rgba()));
        QFont font;
        font.setPixelSize(font_size_);
        painter->setFont(font);
        //draw gear shift position
        draw_gear_shift_(painter, Hud, 0.15, 0.07);
        draw_operation_status_(painter, Hud, 0.51, 0.07);
        bool right_lamp_status = false;
        bool left_lamp_status = false;
        if(last_status_data_->lamp == last_status_data_->LAMP_HAZARD){
            right_lamp_status = true;
            left_lamp_status = true;
        }
        else if(last_status_data_->lamp == last_status_data_->LAMP_LEFT){
            right_lamp_status = false;
            left_lamp_status = true;
        }
        else if(last_status_data_->lamp == last_status_data_->LAMP_RIGHT){
            right_lamp_status = true;
            left_lamp_status = false;
        }
        draw_left_lamp_(painter, Hud, 0.05, 0.05, right_lamp_status);
        draw_right_lamp_(painter, Hud, 0.95, 0.05, left_lamp_status);
        draw_steering_angle_(painter, Hud, 0.19, 0.55);
        draw_steering_mode_(painter, Hud, 0.058, 0.20);
        draw_drive_mode_(painter, Hud, 0.53, 0.20);
        draw_speed_(painter, Hud, 0.58, 0.40);
        draw_brake_bar_(painter, Hud, 0.28, 0.63);
        draw_accel_bar_(painter, Hud, 0.68, 0.63);
        draw_steering_(painter, Hud, 0.26, 0.37);
        return;
    }

    void VehicleStatusMonitor::draw_accel_bar_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        double bar_width = 0.15;
        double bar_height = 0.28;
        QPointF position(width_*x,height_*y);
        QPointF frame_points[4] = {position+QPointF(bar_width/2.0*width_,0),position+QPointF(-bar_width/2.0*width_,0),
            position+QPointF(-bar_width/2.0*width_,bar_height*height_),position+QPointF(bar_width/2.0*width_,bar_height*height_)};
        painter->drawConvexPolygon(frame_points, 4);
        painter->setBrush(QBrush(QColor(0,255,255,(int)(255*alpha_)), Qt::SolidPattern));
        double accel_ratio = (last_status_data_->drivepedal-(double)min_accel_value_)/((double)max_accel_value_-(double)min_accel_value_);
        QPointF bar_points[4] = {position+QPointF(bar_width/2.0*width_,bar_height*height_*(1.0-accel_ratio)),position+QPointF(-bar_width/2.0*width_,bar_height*height_*(1.0-accel_ratio)),
            position+QPointF(-bar_width/2.0*width_,bar_height*height_),position+QPointF(bar_width/2.0*width_,bar_height*height_)};
        painter->drawConvexPolygon(bar_points, 4);
        painter->setBrush(QBrush(QColor(0,0,0,10), Qt::SolidPattern));
        painter->drawText(position+QPointF(-30*width_ratio_,bar_height*height_+20*height_ratio_),QString("ACCEL"));
        return;
    }

    void VehicleStatusMonitor::draw_brake_bar_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        double bar_width = 0.15;
        double bar_height = 0.28;
        QPointF position(width_*x,height_*y);
        QPointF frame_points[4] = {position+QPointF(bar_width/2.0*width_,0),position+QPointF(-bar_width/2.0*width_,0),
            position+QPointF(-bar_width/2*width_,bar_height*height_),position+QPointF(bar_width/2*width_,bar_height*height_)};
        painter->drawConvexPolygon(frame_points, 4);
        painter->setBrush(QBrush(QColor(0,255,255,(int)(255*alpha_)), Qt::SolidPattern));
        double brake_ratio = (last_status_data_->brakepedal-(double)min_brake_value_)/((double)max_brake_value_-(double)min_brake_value_);
        QPointF bar_points[4] = {position+QPointF(bar_width/2.0*width_,bar_height*height_*(1.0-brake_ratio)),position+QPointF(-bar_width/2.0*width_,bar_height*height_*(1.0-brake_ratio)),
            position+QPointF(-bar_width/2.0*width_,bar_height*height_),position+QPointF(bar_width/2.0*width_,bar_height*height_)};
        painter->drawConvexPolygon(bar_points, 4);
        painter->setBrush(QBrush(QColor(0,0,0,10), Qt::SolidPattern));
        painter->drawText(position+QPointF(-30*width_ratio_,bar_height*height_+20*height_ratio_),QString("BRAKE"));
        return;
    }

    void VehicleStatusMonitor::draw_speed_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        double speed;
        QFont font;
        font.setPixelSize((int)((double)font_size_*2.0));
        painter->setFont(font);
        if(speed_unit_ == KM_PER_HOUR){
            speed = last_status_data_->speed;
        }
        else if(speed_unit_ == M_PER_SEC){
            speed = last_status_data_->speed/3.6;
        }
        std::string speed_str = std::to_string(speed);
        int dot_index = -1;
        std::string speed_display_str = "";
        int speed_str_size = speed_str.size();
        for(int i=0; i<speed_str_size; i++){
            if('.' == speed_str[i]){
                dot_index = i;
            }
            if(dot_index!=-1 && i == 3+dot_index){
                break;
            }
            speed_display_str.push_back(speed_str[i]);
        }
        QPointF position(width_*x,height_*y);
        painter->drawText(position,QString(speed_display_str.c_str()));
        font.setPixelSize(font_size_);
        painter->setFont(font);
        QString unit_display_str;
        if(speed_unit_ == KM_PER_HOUR){
            unit_display_str = QString("km/h");
        }
        else if(speed_unit_ == M_PER_SEC){
            unit_display_str = QString("m/s");
        }
        QPointF position_unit(width_*(x+0.08),height_*(y+0.05));
        painter->drawText(position_unit,unit_display_str);
        return;
    }

    void VehicleStatusMonitor::draw_drive_mode_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        QPointF position(width_*x,height_*y);
        if(last_status_data_->drivemode == last_status_data_->MODE_MANUAL){
            painter->drawText(position,QString("DRIVE:MANUAL"));
        }
        else if(last_status_data_->drivemode == last_status_data_->MODE_AUTO){
            painter->drawText(position,QString("DRIVE:AUTO"));
        }
        else{
            painter->drawText(position,QString("DRIVE:---"));
        }
        return;

    }

    void VehicleStatusMonitor::draw_steering_angle_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        double angle;
        if(angle_unit_ == RAD){
            angle = last_status_data_->angle/180*M_PI;
        }
        else if(angle_unit_ == DEG){
            angle = last_status_data_->angle;
        }
        std::string steer_str = std::to_string(angle);
        int dot_index = -1;
        std::string steer_display_str = "";
        int steer_str_size = steer_str.size();
        for(int i=0; i<steer_str_size; i++){
            if('.' == steer_str[i]){
                dot_index = i;
            }
            if(dot_index!=-1 && i == 3+dot_index){
                break;
            }
            steer_display_str.push_back(steer_str[i]);
        }
        if(angle_unit_ == RAD){
            steer_display_str = steer_display_str + " rad";
        }
        else if(angle_unit_ == DEG){
            steer_display_str = steer_display_str + " deg";
        }
        QPointF position(width_*x,height_*y);
        painter->drawText(position,QString(steer_display_str.c_str()));
        return;
    }

    void VehicleStatusMonitor::draw_steering_mode_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        QPointF position(width_*x,height_*y);
        if(last_status_data_->steeringmode == last_status_data_->MODE_MANUAL){
            painter->drawText(position,QString("STEER:MANUAL"));
        }
        else if(last_status_data_->steeringmode == last_status_data_->MODE_AUTO){
            painter->drawText(position,QString("STEER:AUTO"));
        }
        else{
            painter->drawText(position,QString("STEER:---"));
        }
        return;
    }

    void VehicleStatusMonitor::draw_steering_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        QPointF steering_center = QPointF(width_*x,height_*y);
        double r = 45.0;
        painter->translate(steering_center);
        QRect circle_rect(-r*width_ratio_, -r*width_ratio_, 2*r*width_ratio_, 2*r*width_ratio_);
        painter->rotate(-1*last_status_data_->angle);
        QPointF points[4] = {QPointF(-20.0*width_ratio_,-5.0*height_ratio_),QPointF(20.0*width_ratio_,-5.0*height_ratio_),
            QPointF(10.0*width_ratio_,15.0*height_ratio_),QPointF(-10.0*width_ratio_,15.0*height_ratio_)};
        painter->drawConvexPolygon(points, 4);
        painter->rotate(last_status_data_->angle);
        painter->drawEllipse(circle_rect);
        painter->translate(-steering_center);
        return;
    }

    void VehicleStatusMonitor::draw_operation_status_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y){
        QPointF position(width_*x,height_*y);
        if(control_mode_ == "")
        {
            painter->drawText(position,QString("UNKNOWN"));
        }
        else
        {
            transform (control_mode_.begin (), control_mode_.end (), control_mode_.begin (), toupper);
            painter->drawText(position,QString(control_mode_.c_str()));
        }
        return;
    }

    void VehicleStatusMonitor::draw_gear_shift_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y)
    {
        QPointF position(width_*x,height_*y);
        if(last_status_data_.get().gearshift == gear_status_.get_drive_value())
            painter->drawText(position,QString("SHIFT:D"));
        else if(last_status_data_.get().gearshift == gear_status_.get_rear_value())
            painter->drawText(position,QString("SHIFT:R"));
        else if(last_status_data_.get().gearshift == gear_status_.get_brake_value())
            painter->drawText(position,QString("SHIFT:B"));
        else if(last_status_data_.get().gearshift == gear_status_.get_neutral_value())
            painter->drawText(position,QString("SHIFT:N"));
        else if(last_status_data_.get().gearshift == gear_status_.get_parking_value())
            painter->drawText(position,QString("SHIFT:P"));
        else
            painter->drawText(position,QString("---"));
        return;
    }

    void VehicleStatusMonitor::draw_right_lamp_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y, bool status){
        QPointF position(width_*x,height_*y);
        QPointF points[3] = {position+QPointF(-10.0*width_ratio_,10.0*height_ratio_),position+QPointF(-10.0*width_ratio_,-10.0*height_ratio_),position+QPointF(0,0.0)};
        if(status == true){
            painter->setPen(QPen(QColor(255,0,0,(int)(255*alpha_)).rgba()));
            //painter->setBrush(QBrush(QColor(0,255,255,(int)(255*alpha_)), Qt::SolidPattern));
            painter->drawConvexPolygon(points, 3);
            //painter->setBrush(QBrush(QColor(0,0,0,10), Qt::SolidPattern));
            painter->setPen(QPen(QColor(0,255,255,(int)(255*alpha_)).rgba()));
        }
        else{
            painter->drawConvexPolygon(points, 3);
        }
        return;
    }

    void VehicleStatusMonitor::draw_left_lamp_(boost::shared_ptr<QPainter> painter, QImage& Hud, double x, double y, bool status){
        QPointF position(width_*x,height_*y);
        QPointF points[3] = {position+QPointF(10.0*width_ratio_,10.0*height_ratio_),position+QPointF(10.0*width_ratio_,-10.0*height_ratio_),position+QPointF(0.0,0.0)};
        if(status == true){
            painter->setPen(QPen(QColor(255,0,0,(int)(255*alpha_)).rgba()));
            //painter->setBrush(QBrush(QColor(0,255,255,(int)(255*alpha_)), Qt::SolidPattern));
            painter->drawConvexPolygon(points, 3);
            //painter->setBrush(QBrush(QColor(0,0,0,10), Qt::SolidPattern));
            painter->setPen(QPen(QColor(0,255,255,(int)(255*alpha_)).rgba()));
        }
        else{
            painter->drawConvexPolygon(points, 3);
        }
        return;
    }

    void VehicleStatusMonitor::processControlMessage(const std_msgs::String::ConstPtr& msg){
        boost::mutex::scoped_lock lock(mutex_);
        control_mode_ = msg->data;
        return;
    }

    void VehicleStatusMonitor::processMessage(const autoware_msgs::VehicleStatus::ConstPtr& msg){
        boost::mutex::scoped_lock lock(mutex_);
        last_status_data_ = *msg;
        draw_required_ = true;
        return;
    }

    void VehicleStatusMonitor::update_ctrl_mode_topic_(){
        boost::mutex::scoped_lock lock(mutex_);
        ctrl_mode_sub_.shutdown();
        last_status_data_ = boost::none;
        ctrl_mode_topic_name_ = status_topic_property_->getTopicStd();
        if (ctrl_mode_topic_name_.length() > 0 && ctrl_mode_topic_name_ != "/")
        {
            ctrl_mode_sub_ = nh_.subscribe(ctrl_mode_topic_name_, 1, &VehicleStatusMonitor::processMessage, this);
        }
        return;
    }

    void VehicleStatusMonitor::update_status_topic_(){
        boost::mutex::scoped_lock lock(mutex_);
        status_sub_.shutdown();
        last_status_data_ = boost::none;
        topic_name_ = status_topic_property_->getTopicStd();
        if (topic_name_.length() > 0 && topic_name_ != "/")
        {
            status_sub_ = nh_.subscribe(topic_name_, 1, &VehicleStatusMonitor::processMessage, this);
        }
        return;
    }

    void  VehicleStatusMonitor::update_top_(){
        boost::mutex::scoped_lock lock(mutex_);
        monitor_top_ = top_property_->getInt();
        return;
    }

    void  VehicleStatusMonitor::update_left_(){
        boost::mutex::scoped_lock lock(mutex_);
        monitor_left_ = left_property_->getInt();
        return;
    }

    void  VehicleStatusMonitor::update_alpha_(){
        boost::mutex::scoped_lock lock(mutex_);
        alpha_ = alpha_property_->getFloat();
        return;
    }

    void VehicleStatusMonitor::onEnable(){
        if (overlay_) {
            overlay_->show();
        }
        if (ctrl_mode_topic_name_.length() > 0 && ctrl_mode_topic_name_ != "/")
        {
            ctrl_mode_sub_ = nh_.subscribe(ctrl_mode_topic_name_, 1, &VehicleStatusMonitor::processMessage, this);
        }
        if (topic_name_.length() > 0 && topic_name_ != "/")
        {
            status_sub_ = nh_.subscribe(topic_name_, 1, &VehicleStatusMonitor::processMessage, this);
        }
        draw_required_ = true;
        return;
    }

    void VehicleStatusMonitor::onDisable(){
        if (overlay_) {
            overlay_->hide();
        }
        ctrl_mode_sub_.shutdown();
        status_sub_.shutdown();
        return;
    }
}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(autoware_rviz_plugins::VehicleStatusMonitor, rviz::Display)
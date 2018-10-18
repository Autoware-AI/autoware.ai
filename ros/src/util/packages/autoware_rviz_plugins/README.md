# Autoware rviz plugins

rviz plugins for Autoware.  
plugins in this package visualize vehicle status and vehicle command.  

## VehicleStatusMonitor
![VehicleStatusMonitor](media/VehicleStatusMonitor.png) 

## description
VehicleStatusMonitor plugins shows Vehicle Status (type:autoware_msgs/VehicleStatus) topic and Control Mode (std_msgs/String) topic.  
Vehicle Status topic usually published by can_status_translator node in autoware_connector package.  
Control Mode topic usually published by twist_gate node in waypoint_folloer package.  

## panel
![Panel](media/Panel.jpg)  
When right and left lamp lights at the same time.  
It means that hazard lamp is lighting.  

## VehicleCmdMonitor
![VehicleStatusMonitor](media/VehicleStatusMonitor.png) 

## description
VehicleStatusMonitor plugins shows Vehicle Cmd (type:autoware_msgs/VehicleCommand) topic and Control Mode (std_msgs/String) topic.  
Vehicle Status topic usually published by  twist_gate node in waypoint_folloer package.  
Control Mode topic usually published by twist_gate node in waypoint_folloer package.  

## panel
panel is same as VehicleStatusMonitor.
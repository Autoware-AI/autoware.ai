#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
  Copyright 2018-2019 Autoware Foundation. All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
"""

import os, sys, math, datetime, tempfile
import rospy, tf, cv2, yaml
import numpy as np
import tf.transformations as tfm
from sensor_msgs.msg import Image
from cv_bridge import CvBridge, CvBridgeError
from autoware_camera_calibration.calibrator import MonoCalibrator
from autoware_camera_calibration.colmap import Colmap

class CameraCalibratorAutomatic:
    def __init__(self):
        self.camera_name            = rospy.get_param("~camera_name", "autoware_camera_calibration")
        self.calibration_method     = rospy.get_param("~calibration_method", "colmap")
        self.trigger_method         = rospy.get_param("~trigger_method", "transform")
        self.collect_image_number   = rospy.get_param("~collect_image_number", 50)
        self.parent_frame           = rospy.get_param("~parent_frame", "world")
        self.child_frame            = rospy.get_param("~child_frame", "base_link")
        self.delta_distance         = rospy.get_param("~delta_distance", 5.0)
        self.delta_rotation         = rospy.get_param("~delta_rotation", 3.0)
        self.delta_time             = rospy.get_param("~delta_time", 1.0)

        self.image_size = [-1, -1]
        self.image_paths = []
        self.image_counter = 0
        self.prev_translation = None
        self.prev_rotation = None
        self.prev_time = None

        datetime_name = datetime.datetime.now().strftime("%Y%m%d%H%M%S")
        self.temporary_directory = "{}/cameracalibrator_{}".format(tempfile.gettempdir(), datetime_name)

        if self.calibration_method == "colmap":
            self.sfm = Colmap(self.temporary_directory)
        else:
            rospy.logerr("%s is not supported method. Please see README.", self.calibration_method)
            sys.exit(-1)

        if not self.sfm.check_available():
            rospy.logerr("%s is not installed. Please see README.", self.calibration_method)
            sys.exit(-1)

        if self.trigger_method == "transform":
            self.trigger = self.transform_trigger
        elif self.trigger_method == "timer":
            self.trigger = self.timer_trigger
        else:
            rospy.logerr("%s is not supported method. Please see README.", self.trigger_method)
            sys.exit(-1)

        os.mkdir(self.temporary_directory)

        self.tf_listener = tf.TransformListener()
        self.image_sub = rospy.Subscriber("image_raw", Image, self.image_callback, queue_size=10)
        self.cv_bridge = CvBridge()

    def transform_trigger(self):
        try:
            self.tf_listener.waitForTransform(self.child_frame, self.parent_frame, rospy.Time(0), rospy.Duration(1.0))
            (translation, rotation) = self.tf_listener.lookupTransform(self.child_frame, self.parent_frame, rospy.Time(0))
            translation = np.array(translation)
            rotation = np.array(rotation)

            def diff_translation(t1, t2):
                return np.linalg.norm(t1 - t2)

            def diff_rotation(r1, r2):
                dq = tfm.quaternion_multiply(r2, tfm.quaternion_inverse(r1))
                return abs(tfm.euler_from_quaternion(dq)[2])

            if self.prev_translation is not None and self.prev_rotation is not None:
                dt = diff_translation(self.prev_translation, translation)
                dr = diff_rotation(self.prev_rotation, rotation)
                ttrig = (dt > self.delta_distance)
                rtrig = (dr > math.radians(self.delta_rotation))
                if ttrig or rtrig:
                    self.prev_translation = translation
                    self.prev_rotation = rotation
                    return True
                else:
                    return False
            else:
                self.prev_translation = translation
                self.prev_rotation = rotation

            return True

        except Exception as e:
            rospy.logerr("%s", e)
            return False

    def timer_trigger(self):
        now = rospy.Time.now()

        if self.prev_time is not None:
            if (now - self.prev_time).to_sec() > self.delta_time:
                self.prev_time = now
                return True
            else:
                return False
        else:
            self.prev_time = now
            return True

    def image_callback(self, msg):
        try:
            image = self.cv_bridge.imgmsg_to_cv2(msg, "bgr8")

            if self.image_counter == 0:
                self.image_size = (image.shape[1], image.shape[0])

            if self.trigger():
                image_path = "{}/{:0>5}.png".format(self.temporary_directory, self.image_counter)
                self.image_paths.append(image_path)
                cv2.imwrite(image_path, image)
                rospy.loginfo("Correcting image %d / %d -> %s", self.image_counter, self.collect_image_number, image_path)
                self.image_counter += 1

                if self.image_counter > self.collect_image_number:
                    self.image_sub.unregister()
                    success = self.calibration(self.camera_name, self.image_paths, self.image_size)
                    sys.exit(0 if success else -1)

        except Exception as e:
            rospy.logerr("%s", e)

    def calibration(self, camera_name, image_paths, image_size):
        (success, camera_matrix, dist_coeffs) = self.sfm.execute_sfm(image_paths)

        if not success:
            rospy.logerr("Failed to exectute Structure from Motion. Please see README.")
            return False

        try:
            MonoCalibrator.dump_autoware_cv_yaml(camera_name, image_size, camera_matrix, dist_coeffs, 0.0)
        except Exception as e:
            rospy.logerr("%s", e)
            return False

        return True

if __name__ == "__main__":
    rospy.init_node("cameracalibrator_automatic", anonymous=True)
    node = CameraCalibratorAutomatic()
    rospy.spin()

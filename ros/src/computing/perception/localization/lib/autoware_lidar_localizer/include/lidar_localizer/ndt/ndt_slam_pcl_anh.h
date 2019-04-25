/*
 * Copyright 2015-2019 Autoware Foundation. All rights reserved.
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

#ifndef NDT_SLAM_PCL_ANH_H
#define NDT_SLAM_PCL_ANH_H

#include "lidar_localizer/ndt/ndt_slam_base.h"

#include <ndt_cpu/NormalDistributionsTransform.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>

template <class PointSource, class PointTarget>
class NdtSlamPCLANH : public NdtSlamBase<PointSource, PointTarget> {
public:
  NdtSlamPCLANH();
  ~NdtSlamPCLANH() = default;

  void setTransformationEpsilon(double trans_eps) override;
  void setStepSize(double step_size) override;
  void setResolution(float res) override;
  void setMaximumIterations(int max_iter) override;

  double getTransformationEpsilon() override;
  double getStepSize() const override;
  float getResolution() const override;
  int getMaximumIterations() override;
  double getTransformationProbability() const override;

protected:
  void align(const Pose &predict_pose) override;
  double getFitnessScore() override;
  void setInputTarget(
      const boost::shared_ptr<pcl::PointCloud<PointTarget>> &map_ptr) override;
  void setInputSource(
      const boost::shared_ptr<pcl::PointCloud<PointSource>> &scan_ptr) override;
  Pose getFinalPose() override;
  void buildMap(
      const boost::shared_ptr<pcl::PointCloud<PointTarget>> &map_ptr) override;
  void swapInstance() override;

private:
  boost::shared_ptr<cpu::NormalDistributionsTransform<PointSource, PointTarget>>
      ndt_ptr_;
  boost::shared_ptr<cpu::NormalDistributionsTransform<PointSource, PointTarget>>
      swap_ndt_ptr_;
  double score;
};

#endif

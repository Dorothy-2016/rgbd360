/*
 *  Copyright (c) 2013, Universidad de Málaga  - Grupo MAPIR
 *                      INRIA Sophia Antipolis - LAGADIC Team
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of the holder(s) nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: Eduardo Fernandez-Moral
 */

#ifndef CALIB360_H
#define CALIB360_H

#include "definitions.h"

//#include <mrpt/base.h>
#include <clams/discrete_depth_distortion_model.h>

/*! This class contains the functionality to load the calibration parameters of the omnidirectional RGB-D device (RGBD360).
 *  The intrinsic calibration is loaded from previous models obtained with CLAMS (http://cs.stanford.edu/people/teichman/octo/clams/).
 *  The extrinsic calibration is loaded from the models obtained with the Calibration programs.
 */
class Calib360
{

 public:

  /*! The intrinsic depth distortion models corresponding to each Asus XPL */
  std::vector<clams::DiscreteDepthDistortionModel> intrinsic_model_;

  /*! The extrinsic parameters (relative position) of to each Asus XPL */
  Eigen::Matrix4f Rt_[NUM_ASUS_SENSORS];

  /*! The inverse matrices of each Asus XPL's relative position */
  Eigen::Matrix4f Rt_inv[NUM_ASUS_SENSORS];

  /*! The intrinsic pinhole camera model. WARNING: this is set fdepending on the chosen resolution (we use 320x240) */
  Eigen::Matrix3f cameraMatrix;

  /*! Resolution mode of the device (default is QVGA = 320x240) */
  enum Resolution
  {
    VGA = 1,
    QVGA = 2,
    QQVGA = 4,
  } resolution;

  /*! Constructor */
  Calib360(Resolution res=QVGA) : // TODO: set different resolutions properly
    resolution(res)
  {
    // WARNING: cameraMatrix is set depending on the chosen resolution (we use 320x240)
    if(resolution == QVGA)
      cameraMatrix << 262.5, 0., 1.5950e+02,
                      0., 262.5, 1.1950e+02,
                      0., 0., 1.;
//    else if(resolution == QQVGA)
//      cameraMatrix << 131.25, 0., 7.950e+01,
//                      0., 131.25, 5.950e+01,
//                      0., 0., 1.;
//    else if(resolution == VGA)
//      cameraMatrix << 525, 0., 3.1950e+02,
//                      0., 525, 2.3950e+02,
//                      0., 0., 1.;
    else
      std::cout << "\n ERROR -> Set a valid resolution for the device.\n";
  }

  /*! Return the relative position of the camera 'id' */
  inline Eigen::Matrix4f getRt_id(int id) const
  {
    assert(id >= 0 && id < NUM_ASUS_SENSORS);
    return Rt_[id];
  }

  /*! Stitch both the RGB and the depth images corresponding to the sensor 'sensor_id' */
  inline void setRt_id(int id, const Eigen::Matrix4f &Rt)
  {
    Rt_[id] = Rt;
  }

  /*! Load the intrinsic calibration models corresponding to each Asus XPL */
  void loadIntrinsicCalibration(std::string pathToIntrinsicModel = "")
  {
    if(pathToIntrinsicModel == "")
      pathToIntrinsicModel = mrpt::format("%s/Calibration/Intrinsics", PROJECT_SOURCE_PATH);
    std::cout << "Load intrinsic model from " << pathToIntrinsicModel << std::endl;

    intrinsic_model_.resize(NUM_ASUS_SENSORS);
    for(unsigned sensor_id=0; sensor_id<NUM_ASUS_SENSORS; sensor_id++)
    {
//    std::cout << "Load intrinsic model " << sensor_id+1 << std::endl;registerer(mrpt::format("%s/config_files/configLocaliser_sphericalOdometry.ini", PROJECT_SOURCE_PATH));
      intrinsic_model_[sensor_id].load(mrpt::format("%s/distortion_model%i", pathToIntrinsicModel.c_str(), sensor_id+1));
      //int resolution_downsampling = static_cast<int>(resolution);
      intrinsic_model_[sensor_id].downsampleParams(2);//(resolution);
//      std::cout << intrinsic_model_[sensor_id].status() << std::endl;
    }
  }

  /*! Load the extrinsic calibration matrices (relative poses) corresponding to each Asus XPL */
  void loadExtrinsicCalibration(std::string pathToExtrinsicModel = "")
  {
      // Apply offset so that the sensor 4 looks forward
      float pi = 3.14159265359;
      float angle_offset_90 = -90; // X -> right
      Eigen::Matrix4f rot_offset_90 = Eigen::Matrix4f::Identity(); rot_offset_90(0,0) = rot_offset_90(1,1) = cos(angle_offset_90*pi/180); rot_offset_90(0,1) = -sin(angle_offset_90*pi/180); rot_offset_90(1,0) = -rot_offset_90(0,1);
      float angle_offset = 45; //0; //-22.5; //45;
      Eigen::Matrix4f rot_offset = Eigen::Matrix4f::Identity(); rot_offset(1,1) = rot_offset(2,2) = cos(angle_offset*pi/180); rot_offset(1,2) = -sin(angle_offset*pi/180); rot_offset(2,1) = -rot_offset(1,2);
      rot_offset = rot_offset_90 * rot_offset;
      //std::cout << "rot_offset \n" << rot_offset << std::endl;

      if(pathToExtrinsicModel == "")
          pathToExtrinsicModel = mrpt::format("%s/Calibration/Extrinsics", PROJECT_SOURCE_PATH);
      for(unsigned sensor_id=0; sensor_id < NUM_ASUS_SENSORS; ++sensor_id)
      {
          Rt_[sensor_id].loadFromTextFile(mrpt::format("%s/Rt_0%u.txt", pathToExtrinsicModel.c_str(), sensor_id+1));
          Rt_inv[sensor_id] = Rt_[sensor_id].inverse();

          // TODO: use the same reference system in every code!
          // We change the reference system to make it coincide with that used in our matlab program
          Rt_[sensor_id] = rot_offset * Rt_[sensor_id];
          //std::cout << sensor_id << " Rt_ \n" << Rt_[sensor_id] << std::endl;
      }
  }
};

#endif

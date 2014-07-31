/*
 *  Copyright (c) 2012, Universidad de Málaga - Grupo MAPIR
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
 *  Author: Eduardo Fernandez-Moral
 */

#ifndef CLOUD_VISUALIZATION
#define CLOUD_VISUALIZATION

#include <CloudRGBD.h>

#include <pcl/visualization/cloud_viewer.h>
#include <boost/thread/thread.hpp>

/*! This class creates a visualizer to display a CloudRGBD object. The visualizer runs in a separate thread,
 *  and can be sychronized using the mtx_visualization.
 */
class CloudVisualization
{
 public:
  /*! Constructor */
  CloudVisualization(CloudRGBD *cloud = NULL) :
    cloudRGBD(cloud),
    cloudViewer("PointCloud"),
    bUndistortCloud(false)
  {
    run();
  };

  /*! Point Cloud  from RGB-D image to be displayed */
  CloudRGBD *cloudRGBD;

  /*! Mutex to syncrhronize eventual changes in cloudRGBD */
  boost::mutex mtx_visualization;

  /*! Visualizer object */
  pcl::visualization::CloudViewer cloudViewer;

  /*! Run the visualization in a different thread */
  void run()
  {
    cloudViewer.runOnVisualizationThread (boost::bind(&CloudVisualization::viz_cb, this, _1), "viz_cb");
//    cloudViewer.registerKeyboardCallback ( &CloudVisualization::keyboardEventOccurred, *this );
  }

 private:

  /*! Switch between regular/undistorted depth */
  bool bUndistortCloud;

  /*! Get events from the keyboard */
  void keyboardEventOccurred(const pcl::visualization::KeyboardEvent& event, void* viewer_void)
  {
//    if (event.getKeySym() == "u" && event.keyDown())
//      bUndistortCloud = true;
  }

  /*! Visualization callback */
  void viz_cb (pcl::visualization::PCLVisualizer& viz)
  {
    boost::this_thread::sleep (boost::posix_time::milliseconds (10));

    if(cloudRGBD == NULL || cloudRGBD->getPointCloud()->empty())
    {
//      printf("cloudRGBD is a NULL pointer\n");
//      boost::this_thread::sleep (boost::posix_time::milliseconds (10));
      return;
    }

    {
      boost::mutex::scoped_lock lock (mtx_visualization);

      // Render the data
      viz.removeAllPointClouds();

      if (!viz.updatePointCloud (cloudRGBD->getPointCloud(), "cloud"))
        viz.addPointCloud (cloudRGBD->getPointCloud(), "cloud");
    }
  }

};
#endif

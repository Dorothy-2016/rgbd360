/*
 *  Copyright (c) 2012, Universidad de Málaga - Grupo MAPIR and
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
 *  Author: Eduardo Fernandez-Moral
 */

#ifndef MAP360_VISUALIZER_H
#define MAP360_VISUALIZER_H

//#define RECORD_VIDEO 0

#include "Map360.h"
#include <pcl/visualization/cloud_viewer.h>
#include <boost/thread/thread.hpp>

/*! This class creates a visualizer to display a Map360 object. The visualization runs in a separate thread.
 */
class Map360_Visualizer
{
public:

    /*! Reference to the map containing a pose-graph of spherical keyframes */
    Map360 &Map;

    /*! Draw the optimized pose-graph */
    bool bGraphSLAM;

    /*! Freeze the viewer */
    bool bFreezeFrame;

    /*! Draw the camera's current location */
    bool bDrawCurrentLocation;

    /*! Switch between one of the 4 different visualization modes:
     * 0 (Voxelized point cloud),
     * 1 (Overlapping point clouds),
     * 2 (Main keyframe),
     * 3 (Keyframe topology) */
    int nVizMode;

    /*! The index of the pseudo-nearest SphereKF to the camera's current location */
    int currentSphere;

    /*! The pose of the camera's current location */
    Eigen::Affine3f currentLocation;

    /*! Vector of the Keyframe's centers globally referenced */
    std::vector<pcl::PointXYZ> sphere_centers;

    /*! Global point cloud of the map (each keyframe's point cloud is filtered and merged here) */
    pcl::PointCloud<PointT>::Ptr globalMap;

    //    bool bDrawPlanes;

    /*! Global PbMap integrating all the observed planes */ // TODO: probabilistic matching
    mrpt::pbmap::PbMap gobalPbMap;

    /*! Visualizer object */
    pcl::visualization::CloudViewer viewer;

public:

    /*! Constructor. It starts the visualization in a separate thread */
    Map360_Visualizer(Map360 &map, int viz_mode = 0) :
        Map(map),
        viewer("Map360"),
        globalMap(new pcl::PointCloud<PointT>),
        bDrawCurrentLocation(false),
        nVizMode(viz_mode),
        currentSphere(-1),
        bFreezeFrame(false),
        bGraphSLAM(false),
        numScreenshot(0)
    {
        viewer.runOnVisualizationThread (boost::bind(&Map360_Visualizer::viz_cb, this, _1), "viz_cb");
        viewer.registerKeyboardCallback (&Map360_Visualizer::keyboardEventOccurred, *this);
    }

    /*! Index of visualizer screenshot. This is used to create videos with the results */
    int numScreenshot;

    /*! Mutex to syncrhronize eventual changes in the map */
    boost::mutex visualizationMutex;

    /*! Visualization callback */
    void viz_cb (pcl::visualization::PCLVisualizer& viz)
    {
        //    std::cout << "Map360_Visualizer::viz_cb(...)\n";
        if (Map.vpSpheres.size() == 0 || bFreezeFrame)
        {
            boost::this_thread::sleep (boost::posix_time::milliseconds (10));
            return;
        }

        //    viz.setFullscreen(true);
        viz.removeAllShapes();
        viz.removeAllPointClouds();
        viz.removeCoordinateSystem("camera");

        {
            boost::mutex::scoped_lock updateLockVisualizer(visualizationMutex);
            boost::mutex::scoped_lock updateLock(Map.mapMutex);

            char name[1024];

            sprintf (name, "Frames %lu. Graph-SLAM %d", Map.vpSpheres.size(), bGraphSLAM);
            viz.addText (name, 20, 20, "params");

            // Draw the current camera
            if(bDrawCurrentLocation)
                viz.addCoordinateSystem(0.2, currentLocation, "camera");

            if(nVizMode == 0)
            {
              if(globalMap->size() > 0)
              {
                if (!viz.updatePointCloud (globalMap, "globalMap"))
                  viz.addPointCloud (globalMap, "globalMap");
              }
            }
            else if(nVizMode == 1)
            {
//            cout << "Draw pointClouds " << Map.vTrajectoryPoses.size() << endl;
                // Draw the overlapping keyframe's point clouds
                if(!bGraphSLAM)
                {
                    //        for(unsigned i=0; i < Map.vpSpheres.size(); i++)
                    for(unsigned i=0; i < Map.vTrajectoryPoses.size(); i++)
                    {
                        sprintf (name, "cloud%u", i);
                        if (!viz.updatePointCloud (Map.vpSpheres[i]->sphereCloud, name))
                            viz.addPointCloud (Map.vpSpheres[i]->sphereCloud, name);
                        Eigen::Affine3f Rt;
                        Rt.matrix() = Map.vTrajectoryPoses[i];//.cast<float>();
                        viz.updatePointCloudPose(name, Rt);
                    }
                }
                else
                {
                    for(unsigned i=0; i < Map.vOptimizedPoses.size(); i++)
                    {
                        sprintf (name, "cloud%u", i);
                        if (!viz.updatePointCloud (Map.vpSpheres[i]->sphereCloud, name))
                            viz.addPointCloud (Map.vpSpheres[i]->sphereCloud, name);
                        Eigen::Affine3f Rt;
                        Rt.matrix() = Map.vOptimizedPoses[i];//.cast<float>();
                        viz.updatePointCloudPose(name, Rt);
                    }
                }
            }
            else if(nVizMode == 2) // Draw the keyframes
            {
                for(unsigned i=0; i< Map.vSelectedKFs.size(); i++)
                {
                    sprintf (name, "KF%u", Map.vSelectedKFs[i]);
                    if (!viz.updatePointCloud (Map.vpSpheres[Map.vSelectedKFs[i]]->sphereCloud, name))
                        viz.addPointCloud (Map.vpSpheres[Map.vSelectedKFs[i]]->sphereCloud, name);
                    Eigen::Affine3f Rt;
//                    Rt.matrix() = Map.vTrajectoryPoses[Map.vSelectedKFs[i]];
                    Rt.matrix() = Map.vOptimizedPoses[Map.vSelectedKFs[i]];
                    viz.updatePointCloudPose(name, Rt);
                }
            }

            // Draw sphere locations
            assert( Map.vTrajectoryPoses.size() == Map.vpSpheres.size() );
            pcl::PointXYZ pt_center;//, pt_center_prev;
            if(!bGraphSLAM)
            {
                //if(Map.vsAreas.size() > 1) cout << "Draw spheres " << Map.vTrajectoryPoses.size() << endl;
                sphere_centers.resize(Map.vTrajectoryPoses.size());
                //        for(unsigned i=0; i < Map.vpSpheres.size(); i++)
                for(unsigned i=0; i < Map.vTrajectoryPoses.size(); i++)
                {
                    pt_center = pcl::PointXYZ(Map.vTrajectoryPoses[i](0,3), Map.vTrajectoryPoses[i](1,3), Map.vTrajectoryPoses[i](2,3));
                    sphere_centers[i] = pt_center;
                    sprintf (name, "pose%u", i);
                    viz.addSphere (pt_center, 0.04, ared[Map.vpSpheres[i]->node%10], agrn[Map.vpSpheres[i]->node%10], ablu[Map.vpSpheres[i]->node%10], name);

                    sprintf (name, "%u", i);
                    pt_center.x += 0.05;
                    viz.addText3D (name, pt_center, 0.05, ared[Map.vpSpheres[i]->node%10], agrn[Map.vpSpheres[i]->node%10], ablu[Map.vpSpheres[i]->node%10], name);
                }

                // Draw the locations of the keyframes
                for(unsigned i=0; i< Map.vSelectedKFs.size(); i++)
                {
                    //if(Map.vsAreas.size() > 1) cout << " Draw sphere " << i << " " << Map.vSelectedKFs[i] << endl;
                    pt_center = pcl::PointXYZ(Map.vTrajectoryPoses[Map.vSelectedKFs[i]](0,3), Map.vTrajectoryPoses[Map.vSelectedKFs[i]](1,3), Map.vTrajectoryPoses[Map.vSelectedKFs[i]](2,3));
                    sprintf (name, "poseKF%u", i);
                    viz.addSphere (pt_center, 0.1, ared[i%10], agrn[i%10], ablu[i%10], name);
                }
            }
            else
            {
                sphere_centers.resize(Map.vOptimizedPoses.size());
                for(unsigned i=0; i < Map.vOptimizedPoses.size(); i++)
                {
                    pt_center = pcl::PointXYZ(Map.vOptimizedPoses[i](0,3), Map.vOptimizedPoses[i](1,3), Map.vOptimizedPoses[i](2,3));
                    sphere_centers[i] = pt_center;
                    sprintf (name, "pose%u", i);
                    viz.addSphere (pt_center, 0.04, ared[Map.vpSpheres[i]->node%10], agrn[Map.vpSpheres[i]->node%10], ablu[Map.vpSpheres[i]->node%10], name);

                    sprintf (name, "%u", i);
                    pt_center.x += 0.05;
                    viz.addText3D (name, pt_center, 0.05, ared[Map.vpSpheres[i]->node%10], agrn[Map.vpSpheres[i]->node%10], ablu[Map.vpSpheres[i]->node%10], name);
                }

                // Draw the locations of the keyframes
                for(unsigned i=0; i< Map.vSelectedKFs.size(); i++)
                {
                    pt_center = pcl::PointXYZ(Map.vOptimizedPoses[Map.vSelectedKFs[i]](0,3), Map.vOptimizedPoses[Map.vSelectedKFs[i]](1,3), Map.vOptimizedPoses[Map.vSelectedKFs[i]](2,3));
                    sprintf (name, "poseKF%u", i);
                    viz.addSphere (pt_center, 0.1, ared[i%10], agrn[i%10], ablu[i%10], name);
                }
            }

            // Draw edges
            for(std::map<unsigned, std::map<unsigned, std::pair<Eigen::Matrix4f, Eigen::Matrix<float,6,6> > > >::iterator it1=Map.mmConnectionKFs.begin();
                it1 != Map.mmConnectionKFs.end(); it1++)
                for(std::map<unsigned, std::pair<Eigen::Matrix4f, Eigen::Matrix<float,6,6> > >::iterator it2=it1->second.begin(); it2 != it1->second.end(); it2++)
                {
                    //          std::cout << " Draw links " << it1->first << " " << it2->first << "\n";
                    sprintf (name, "link%u_%u", it1->first, it2->first);
                    viz.addLine (sphere_centers[it1->first], sphere_centers[it2->first], name);
                }

            //      bFreezeFrame = true;

#if RECORD_VIDEO
            std::string screenshotFile = mrpt::format("im_%04u.png", ++numScreenshot);
            viz.saveScreenshot (screenshotFile);
#endif

            updateLock.unlock();
        }
    }

    /*! Get events from the keyboard */
    void keyboardEventOccurred (const pcl::visualization::KeyboardEvent &event, void* viewer_void)
    {
        if ( event.keyDown () )
        {
            //      std::cout << "Key pressed " << event.getKeySym () << endl;
            if(event.getKeySym () == "k" || event.getKeySym () == "K")
                bFreezeFrame = !bFreezeFrame;
            else if(event.getKeySym () == "l" || event.getKeySym () == "L")
                bGraphSLAM = !bGraphSLAM;
            else if(event.getKeySym () == "m" || event.getKeySym () == "M")
            {
                nVizMode = (nVizMode+1) % 4;
                std::cout << "Visualizatio swap to mode " << nVizMode << endl;
            }
        }
    }

};

#endif

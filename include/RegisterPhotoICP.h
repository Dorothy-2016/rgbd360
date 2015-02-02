/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2014, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */

#ifndef REGISTER_PHOTO_ICP_H
#define REGISTER_PHOTO_ICP_H

#define ENABLE_OPENMP 1
//#define ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS 1

#include "Miscellaneous.h"

#include <mrpt/slam/CSimplePointsMap.h>
#include <mrpt/slam/CObservation2DRangeScan.h>
#include <mrpt/slam/CICP.h>
#include <mrpt/poses/CPose2D.h>
#include <mrpt/poses/CPosePDF.h>
#include <mrpt/poses/CPosePDFGaussian.h>
//#include <mrpt/gui.h>
#include <mrpt/math/utils.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
//#include <opencv2/contrib/contrib.hpp> //TickMeter
#include <iostream>
#include <fstream>

#include <pcl/registration/icp.h>
#include <pcl/registration/icp_nl.h> //ICP LM
#include <pcl/registration/gicp.h> //GICP
#include <pcl/registration/warp_point_rigid.h>

#define INVALID_POINT -10000

//namespace JacobianSorting
//{
//  template<class T>
//  struct CompareDeref
//  {
//    bool operator()( const T& a, const T& b ) const
//      { return *a < *b; }
//  };


//  template<class T, class U>
//  struct PairJacobianIdx
//  {
//    const U& operator()( const std::pair<T,U>& a ) const
//      { return a.second; }
//  };

//  template<class IterIn, class IterOut>
//  void sort_idx( IterIn first, IterIn last, IterOut out )
//  {
//    std::multimap<IterIn, int, CompareDeref<IterIn> > v;
//    for( int i=0; first != last; ++i, ++first )
//      v.insert( std::make_pair( first, i ) );
//    std::transform( v.begin(), v.end(), out, PairJacobianIdx<IterIn const,int>() );
//  }
//}
////std::vector<int> idxtbl( jacobians.rows );
////JacobianSorting::sort_idx( ai, ai+10, idxtbl.begin() );


// Return a diagonal matrix where the values of the diagonal are assigned from the input vector
template<typename typedata, int nRows, int nCols>
Eigen::Matrix<typedata,nRows,nCols> getDiagonalMatrix(const Eigen::Matrix<typedata,nRows,nCols> &matrix_generic)
{
    assert(nRows == nCols);

    Eigen::Matrix<typedata,nRows,nCols> m_diag = Eigen::Matrix<typedata,nRows,nCols>::Zero();
    for(size_t i=0; i < nRows; i++)
        m_diag(i,i) = matrix_generic(i,i);

    return m_diag;
}

class RegisterPhotoICP
{
//public:
    /*! Camera matrix (intrinsic parameters).*/
    Eigen::Matrix3f cameraMatrix;

//    /* Vertical field of view in the sphere (in) .*/
//    float phi_FoV;

    /*! The reference intensity image */
    cv::Mat graySrc;
    cv::Mat rgbSrc;

    /*! The target intensity image */
    cv::Mat grayTrg;

    /*! The reference depth image */
    cv::Mat depthSrc;

    /*! The target depth image */
    cv::Mat depthTrg;

    /*! The relative pose between source and target RGB-D images */
    Eigen::Matrix4f relPose;

    /*! The Hessian matrix of the optimization problem. At the solution it represents the inverse of the covariance matrix of the relative pose */
    Eigen::Matrix<float,6,6> hessian;

    /*! The Gradient vector of the optimization problem. At the solution it should be zero */
    Eigen::Matrix<float,6,1> gradient;

    //    /* Current iteration at the current optimization level.*/
    //    int iter;

    /*! Minimum allowed depth to consider a depth pixel valid.*/
    float minDepth;

    /*! Maximum allowed depth to consider a depth pixel valid.*/
    float maxDepth;

    /*! Minimum depth difference to filter the outliers.*/
    float minDepthOutliers;

    /*! Threshold depth difference to filter the outliers.*/
    float thresDepthOutliers;

    /*! Maximum depth difference to filter the outliers.*/
    float maxDepthOutliers;

    /*! Variance of intensity measurements. */
    float varPhoto;
    float stdDevPhoto;

    /*! Variance of intensity measurements. */
    float varDepth;
    float stdDevDepth;

    /*! Depth component gain. This variable is used to scale the depth values so that depth components are similar to intensity values.*/
    float depthComponentGain;

    /*! If set to true, only the pixels with high gradient in the gray image are used (for both photo and depth minimization) */
    bool bUseSalientPixels;

    /*! A threshold to select salient pixels on the intensity and depth images.*/
    float thresSaliency;
    float thresSaliencyIntensity;
    float thresSaliencyDepth;

//    /*! Sensed-Space-Overlap of the registered frames. This is the relation between the co-visible pixels and the total number of pixels in the image.*/
//    float SSO;

    /*! If set to true, only the pixels with high gradient in the gray image are used (for both photo and depth minimization) */
    std::vector<std::vector<int> > vSalientPixels;

    /*! Enable the visualization of the optimization process (only for debug).*/
    bool visualizeIterations;

    /*! Warped intensity image for visualization purposes.*/
    cv::Mat warped_source_grayImage;

    /*! Warped intensity image for visualization purposes.*/
    cv::Mat warped_source_depthImage;

    /*! Vector containing the indices of the pixels that move (forward-backward) or are occluded in the target image to the source image.*/
    cv::Mat mask_dynamic_occlusion;

    /*! LUT of 3D points of the spherical image.*/
    std::vector<Eigen::Vector3f> LUT_xyz_sphere;

    /*! Number of iterations in each pyramid level.*/
    std::vector<int> num_iterations;

public:

    /*! Sensed-Space-Overlap of the registered frames. This is the relation between the co-visible pixels and the total number of pixels in the image.*/
    float SSO;

    /*! The average residual per pixel of photo/depth consistency.*/
    float avResidual;

    /*! The average residual per pixel of photo consistency.*/
    double avPhotoResidual;

    /*! The average residual per pixel of depth consistency.*/
    double avDepthResidual;

    /*! Number of pyramid levels.*/
    int nPyrLevels;

    /*! Optimization method (cost function). The options are: 0=Photoconsistency, 1=Depth consistency (ICP like), 2= A combination of photo-depth consistency */
    enum costFuncType {PHOTO_CONSISTENCY, DEPTH_CONSISTENCY, PHOTO_DEPTH} method;

    /*! Intensity (gray), depth and gradient image pyramids. Each pyramid has 'numpyramidLevels' levels.*/
    std::vector<cv::Mat> graySrcPyr, grayTrgPyr, depthSrcPyr, depthTrgPyr, grayTrgGradXPyr, grayTrgGradYPyr, depthTrgGradXPyr, depthTrgGradYPyr;
    std::vector<cv::Mat> colorSrcPyr;

    RegisterPhotoICP() :
        minDepth(0.3),
        maxDepth(6.0),
        nPyrLevels(4),
        bUseSalientPixels(false),
        visualizeIterations(false)
    {
        stdDevPhoto = 6./255;
        varPhoto = stdDevPhoto*stdDevPhoto;

        stdDevDepth = 0.2;
        varDepth = stdDevDepth*stdDevDepth;

        minDepthOutliers = 2*stdDevDepth; // in meters
        maxDepthOutliers = 1; // in meters

        thresSaliency = 0.01f;
        thresSaliencyIntensity = 0.01f;
        thresSaliencyDepth = 0.01f;
        vSalientPixels.resize(nPyrLevels);
    };

    /*! Set the the number of pyramid levels.*/
    void setNumPyr(int Npyr)
    {
        nPyrLevels = Npyr;
    };

    /*! Set the minimum depth distance (m) to consider a certain pixel valid.*/
    void setMinDepth(float minD)
    {
        minDepth = minD;
    };

    /*! Set the maximum depth distance (m) to consider a certain pixel valid.*/
    void setMaxDepth(float maxD)
    {
        maxDepth = maxD;
    };

    /*! Set the variance of the intensity.*/
    void setGrayVariance(float stdDev)
    {
        stdDevPhoto = stdDev;
    };

    /*! Set the variance of the depth.*/
    void setDepthVariance(float stdDev)
    {
        stdDevDepth = stdDev;
    };

    /*! Set the 3x3 matrix of (pinhole) camera intrinsic parameters used to obtain the 3D colored point cloud from the RGB and depth images.*/
    void setCameraMatrix(Eigen::Matrix3f & camMat)
    {
        cameraMatrix = camMat;
    };

    /*! Set the visualization of the optimization progress.*/
    void setVisualization(bool viz)
    {
        visualizeIterations = viz;
    };

    /*! Set the a variable to indicate whether pixel saliency is used.*/
    void useSaliency(bool bUseSalientPixels_)
    {
        bUseSalientPixels = bUseSalientPixels_;
    };

    /*! Returns the optimal SE(3) rigid transformation matrix between the source and target frame.
     * This method has to be called after calling the alignFrames() method.*/
    Eigen::Matrix4f getOptimalPose()
    {
        return relPose;
    }

    /*! Returns the Hessian (the information matrix).*/
    Eigen::Matrix<float,6,6> getHessian()
    {
        return hessian;
    }

    /*! Returns the Gradient (the information matrix).*/
    Eigen::Matrix<float,6,1> getGradient()
    {
        return gradient;
    }

    /*! Build a pyramid of nLevels of image resolutions from the input image.
     * The resolution of each layer is 2x2 times the resolution of its image above.*/
    void buildPyramid(cv::Mat &img, std::vector<cv::Mat> &pyramid, const int nLevels)
    {
        //Create space for all the images // ??
        pyramid.resize(nLevels);
        pyramid[0] = img;
        //std::cout << "types " << pyramid[0].type() << " " << img.type() << std::endl;

        for(int level=1; level < nLevels; level++)
        {
            //Create an auxiliar image of factor times the size of the original image
            cv::Mat imgAux;
            pyrDown( pyramid[level-1], imgAux, cv::Size( pyramid[level-1].cols/2, pyramid[level-1].rows/2 ) );

            //Assign the resized image to the current level of the pyramid
            pyramid[level]=imgAux;
        }
    };

    /*! Build a pyramid of nLevels of image resolutions from the input image.
     * The resolution of each layer is 2x2 times the resolution of its image above.*/
    void buildPyramidRange(cv::Mat &img, std::vector<cv::Mat> &pyramid, const int nLevels)
    {
        //Create space for all the images // ??
        pyramid.resize(nLevels);
        if(img.type() == CV_16U) // If the image is in milimetres, convert it to meters
            img.convertTo(pyramid[0], CV_32FC1, 0.001 );
        else
            pyramid[0] = img;
//    std::cout << "types " << pyramid[0].type() << " " << img.type() << std::endl;

        for(int level=1; level < nLevels; level++)
        {
            //Create an auxiliar image of factor times the size of the original image
            pyramid[level] = cv::Mat::zeros(cv::Size( pyramid[level-1].cols/2, pyramid[level-1].rows/2 ), pyramid[0].type() );
//            cv::Mat imgAux = cv::Mat::zeros(cv::Size( pyramid[level-1].cols/2, pyramid[level-1].rows/2 ), pyramid[0].type() );
#if ENABLE_OPENMP
#pragma omp parallel for
#endif
            for(unsigned r=0; r < pyramid[level-1].rows; r+=2)
                for(unsigned c=0; c < pyramid[level-1].cols; c+=2)
                {
                    float avDepth = 0.f;
                    unsigned nValidPixels = 0;
                    for(unsigned i=0; i < 2; i++)
                        for(unsigned j=0; j < 2; j++)
                        {
                            float z = pyramid[level-1].at<float>(r+i,c+j);
                        //cout << "z " << z << endl;
                            if(z > minDepth && z < maxDepth)
                            {
                                avDepth += z;
                                ++nValidPixels;
                            }
                        }
                    if(nValidPixels > 0)
                        pyramid[level].at<float>(r/2,c/2) = avDepth / nValidPixels;
//                        imgAux.at<float>(r/2,c/2) = avDepth / nValidPixels;
                }

//            //Assign the resized image to the current level of the pyramid
//            pyramid[level] = imgAux;
        }
    };


    /*! Build the pyramid levels from the intensity images.*/
    void buildGrayPyramids()
    {
        buildPyramid(graySrc, graySrcPyr, nPyrLevels);
        buildPyramid(grayTrg, graySrcPyr, nPyrLevels);
    };

    /*! Calculate the image gradients in X and Y. This gradientes are calculated through weighted first order approximation (as adviced by Mariano Jaimez). */
    void calcGradientXY(cv::Mat &src, cv::Mat &gradX, cv::Mat &gradY)
    {
        int dataType = src.type();

        gradX = cv::Mat::zeros(cv::Size(src.cols, src.rows), src.type() );
        gradY = cv::Mat::zeros(cv::Size(src.cols, src.rows), src.type() );

        for(unsigned r=1; r < src.rows-1; r++)
            for(unsigned c=1; c < src.cols-1; c++)
            {
                if( (src.at<float>(r,c) > src.at<float>(r,c+1) && src.at<float>(r,c) < src.at<float>(r,c-1) ) ||
                    (src.at<float>(r,c) < src.at<float>(r,c+1) && src.at<float>(r,c) > src.at<float>(r,c-1) )   )
                    gradX.at<float>(r,c) = 2.f / (1/(src.at<float>(r,c+1)-src.at<float>(r,c)) + 1/(src.at<float>(r,c)-src.at<float>(r,c-1)));

//                std::cout << "GradX " << gradX.at<float>(r,c) << " " << gradY.at<float>(r,c) << std::endl;
//                if(fabs(gradX.at<float>(r,c)) > 150 || std::isnan(gradX.at<float>(r,c)))
//                {
//                    std::cout << "highGradX " << gradX.at<float>(r,c) << " " << src.at<float>(r,c+1) << " " << src.at<float>(r,c) << " " << src.at<float>(r,c-1) << std::endl;
//                    sleep(1000000);
//                }

                if( (src.at<float>(r,c) > src.at<float>(r+1,c) && src.at<float>(r,c) < src.at<float>(r-1,c) ) ||
                    (src.at<float>(r,c) < src.at<float>(r+1,c) && src.at<float>(r,c) > src.at<float>(r-1,c) )   )
                    gradY.at<float>(r,c) = 2.f / (1/(src.at<float>(r+1,c)-src.at<float>(r,c)) + 1/(src.at<float>(r,c)-src.at<float>(r-1,c)));

//                if(fabs(gradY.at<float>(r,c)) > 150 || std::isnan(gradY.at<float>(r,c)))
//                {
//                    std::cout << "highGrad " << gradY.at<float>(r,c) << " " << src.at<float>(r+1,c) << " " << src.at<float>(r,c) << " " << src.at<float>(r-1,c) << std::endl;
//                    sleep(1000000);
//                }
            }
//        gradX = grad_x;
//        gradY = grad_y;
    };

    /*! Calculate the image gradients in X and Y. This gradientes are calculated through weighted first order approximation (as adviced by Mariano Jaimez). */
    void calcGradientXY_saliency(cv::Mat &src, cv::Mat &gradX, cv::Mat &gradY, std::vector<int> &vSalientPixels_)
    {
        int dataType = src.type();

        gradX = cv::Mat::zeros(cv::Size(src.cols, src.rows), src.type() );
        gradY = cv::Mat::zeros(cv::Size(src.cols, src.rows), src.type() );

        for(unsigned r=1; r < src.rows-1; r++)
            for(unsigned c=1; c < src.cols-1; c++)
            {
                if( (src.at<float>(r,c) > src.at<float>(r,c+1) && src.at<float>(r,c) < src.at<float>(r,c-1) ) ||
                    (src.at<float>(r,c) < src.at<float>(r,c+1) && src.at<float>(r,c) > src.at<float>(r,c-1) )   )
                    gradX.at<float>(r,c) = 2.f / (1/(src.at<float>(r,c+1)-src.at<float>(r,c)) + 1/(src.at<float>(r,c)-src.at<float>(r,c-1)));

                if( (src.at<float>(r,c) > src.at<float>(r+1,c) && src.at<float>(r,c) < src.at<float>(r-1,c) ) ||
                    (src.at<float>(r,c) < src.at<float>(r+1,c) && src.at<float>(r,c) > src.at<float>(r-1,c) )   )
                    gradY.at<float>(r,c) = 2.f / (1/(src.at<float>(r+1,c)-src.at<float>(r,c)) + 1/(src.at<float>(r,c)-src.at<float>(r-1,c)));
            }

        vSalientPixels_.clear();
        for(unsigned r=1; r < src.rows-1; r++)
            for(unsigned c=1; c < src.cols-1; c++)
                if( (fabs(gradX.at<float>(r,c)) > thresSaliency) || (fabs(gradY.at<float>(r,c)) > thresSaliency) )
                    vSalientPixels_.push_back(src.cols*r+c); //vector index
    };

    /*! Compute the gradient images for each pyramid level. */
    //    void buildGradientPyramids(std::vector<cv::Mat>& graySrcPyr,std::vector<cv::Mat>& grayTrgGradXPyr,std::vector<cv::Mat>& grayTrgGradYPyr)
    void buildGradientPyramids()
    {
        //Compute image gradients
//        double scale = 1./255;
//        double delta = 0;
//        int dataType = CV_32FC1; // grayTrgPyr[level].type();

        //Create space for all the derivatives images
        grayTrgGradXPyr.resize(grayTrgPyr.size());
        grayTrgGradYPyr.resize(grayTrgPyr.size());

        depthTrgGradXPyr.resize(grayTrgPyr.size());
        depthTrgGradYPyr.resize(grayTrgPyr.size());

        for(unsigned level=0; level < nPyrLevels; level++)
        {
            if(bUseSalientPixels)
                calcGradientXY_saliency(grayTrgPyr[level], grayTrgGradXPyr[level], grayTrgGradYPyr[level], vSalientPixels[level]);
            else
                calcGradientXY(grayTrgPyr[level], grayTrgGradXPyr[level], grayTrgGradYPyr[level]);

            calcGradientXY(depthTrgPyr[level], depthTrgGradXPyr[level], depthTrgGradYPyr[level]);

            // Compute the gradient in x
//            grayTrgGradXPyr[level] = cv::Mat(cv::Size( grayTrgPyr[level].cols, grayTrgPyr[level].rows), grayTrgPyr[level].type() );
//            cv::Scharr( grayTrgPyr[level], grayTrgGradXPyr[level], dataType, 1, 0, scale, delta, cv::BORDER_DEFAULT );
//            cv::Sobel( grayTrgPyr[level], grayTrgGradXPyr[level], dataType, 1, 0, 3, scale, delta, cv::BORDER_DEFAULT );

            // Compute the gradient in y
//            grayTrgGradYPyr[level] = cv::Mat(cv::Size( grayTrgPyr[level].cols, grayTrgPyr[level].rows), grayTrgPyr[level].type() );
//            cv::Scharr( grayTrgPyr[level], grayTrgGradYPyr[level], dataType, 0, 1, scale, delta, cv::BORDER_DEFAULT );
//            cv::Sobel( grayTrgPyr[level], grayTrgGradYPyr[level], dataType, 0, 1, 3, scale, delta, cv::BORDER_DEFAULT );

//            cv::Mat imgNormalizedDepth;
//            imagePyramid[level].convertTo(imgNormalizedDepth, CV_32FC1,1./maxDepth);

            // Compute the gradient in x
//            cv::Scharr( depthTrgPyr[level], depthTrgGradXPyr[level], dataType, 1, 0, scale, delta, cv::BORDER_DEFAULT );

            // Compute the gradient in y
//            cv::Scharr( depthTrgPyr[level], depthTrgGradYPyr[level], dataType, 0, 1, scale, delta, cv::BORDER_DEFAULT );

//            cv::imshow("DerX", grayTrgGradXPyr[level]);
//            cv::imshow("DerY", grayTrgGradYPyr[level]);
//            cv::waitKey(0);
//            cv::imwrite(mrpt::format("/home/edu/gradX_%d.png",level), grayTrgGradXPyr[level]);
//            cv::imwrite(mrpt::format("/home/edu/gray_%d.png",level), grayTrgPyr[level]);
        }
    };

    /*! Sets the source (Intensity+Depth) frame.*/
    void setSourceFrame(cv::Mat &imgRGB,cv::Mat &imgDepth)
    {
        //Create a float auxialiary image from the imput image
        //        cv::Mat imgGrayFloat;
        rgbSrc = imgRGB;
        cv::cvtColor(imgRGB, graySrc, CV_RGB2GRAY);
        graySrc.convertTo(graySrc, CV_32FC1, 1./255 );

        //Compute image pyramids for the grayscale and depth images
        buildPyramid(graySrc, graySrcPyr, nPyrLevels);
        buildPyramidRange(imgDepth, depthSrcPyr, nPyrLevels);
        //        buildPyramid(imgGrayFloat,gray0Pyr,numnLevels,true);
        //        buildPyramid(imgDepth,depth0Pyr,numnLevels,false);

        buildPyramid(rgbSrc, colorSrcPyr, nPyrLevels);
    };

    /*! Sets the source (Intensity+Depth) frame. Depth image is ignored*/
    void setTargetFrame(cv::Mat &imgRGB,cv::Mat &imgDepth)
    {
        //Create a float auxialiary image from the imput image
        //        cv::Mat imgGrayFloat;
        cv::cvtColor(imgRGB, grayTrg, CV_RGB2GRAY);
        grayTrg.convertTo(grayTrg, CV_32FC1, 1./255 );

        //Compute image pyramids for the grayscale and depth images
        //        buildPyramid(imgGrayFloat,gray1Pyr,numnLevels,true);
        buildPyramid(grayTrg, grayTrgPyr, nPyrLevels);
        buildPyramidRange(imgDepth, depthTrgPyr, nPyrLevels);

        //Compute image pyramids for the gradients images
        buildGradientPyramids();

//        cv::imshow("GradX ", grayTrgGradXPyr[0]);
//        cv::imshow("GradX_d ", depthTrgGradXPyr[0]);
//        cv::waitKey(0);
    };

    /*! Project pixel spherical */
    inline void projectSphere(int &nCols, int &nRows, float &phi_FoV, int &c, int &r, float &depth, Eigen::Matrix4f &poseGuess,
                              int &transformed_r_int, int &transformed_c_int) // output parameters
    {
        float phi = r*(2*phi_FoV/nRows);
        float theta = c*(2*PI/nCols);

        //Compute the 3D coordinates of the pij of the source frame
        Eigen::Vector4f point3D;
        point3D(0) = depth*sin(phi);
        point3D(1) = -depth*cos(phi)*sin(theta);
        point3D(2) = -depth*cos(phi)*cos(theta);
        point3D(3) = 1;

        //Transform the 3D point using the transformation matrix Rt
        Eigen::Vector4f transformedPoint3D = poseGuess*point3D;
        float depth_trg = sqrt(transformedPoint3D(0)*transformedPoint3D(0) + transformedPoint3D(1)*transformedPoint3D(1) + transformedPoint3D(2)*transformedPoint3D(2));

        //Project the 3D point to the S2 sphere
        float phi_trg = asin(transformedPoint3D(2)/depth_trg);
        float theta_trg = atan2(-transformedPoint3D(1),-transformedPoint3D(2));
        transformed_r_int = round(phi_trg*nRows/phi_FoV + nRows/2);
        transformed_c_int = round(theta_trg*nCols/(2*PI));
    };

    /*! Huber weight for robust estimation. */
    template<typename T>
    inline T weightHuber(const T &error, const T &regularization)
    {
        //        assert(!std::isnan(error) && !std::isnan(regularization))
        T error_abs = fabs(error);
        if(error_abs < regularization)
            return 1.;

        T weight = sqrt(2*regularization*error_abs-regularization*regularization) / error_abs;
        return weight;
    };

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    double errorPhotoICP(const int &pyramidLevel, const Eigen::Matrix4f poseGuess, costFuncType method = PHOTO_CONSISTENCY)
    {
        //double error2 = 0.0; // Squared error
        double PhotoResidual = 0.0;
        double DepthResidual = 0.0;
        int nValidPhotoPts = 0;
        int nValidDepthPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

//        float varianceRegularization = 1; // 63%
//        float stdDevReg = sqrt(varianceRegularization);
        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

//    std::cout << "poseGuess \n" << poseGuess << std::endl;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(bUseSalientPixels)
        {
            std::cout << "bUseSalientPixels \n";
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual,nValidPhotoPts,nValidDepthPts) //(+:error2)
#endif        
            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
            {
//                //                int i = nCols*r+c; //vector index
//                int r = vSalientPixels[pyramidLevel][i] / nCols;
//                int c = vSalientPixels[pyramidLevel][i] % nCols;

//                //Compute the 3D coordinates of the pij of the source frame
//                Eigen::Vector3f point3D;
//                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
//                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
//                {
//                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
//                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

//                    //Transform the 3D point using the transformation matrix Rt
//                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

                if(LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                {
                    Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]] + translation;

                    //Project the 3D point to the 2D plane
                    float inv_transformedPz = 1.0/transformedPoint3D(2);
                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);

                    //Asign the intensity value to the warped image and compute the difference between the transformed
                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            //Obtain the pixel values that will be used to compute the pixel residual
                            // float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                            float pixel1 = graySrcPyr[pyramidLevel].data[vSalientPixels[pyramidLevel][i]]; // Intensity value of the pixel(r,c) of source frame
                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                            float weightedErrorPhoto = weight_photo * photoDiff;
                            // Apply M-estimator weighting
//                            if(photoDiff2 > varianceRegularization)
//                            {
////                                float photoDiff2_norm = sqrt(photoDiff2);
////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
//                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
//                            }
                            //error2 += weightedErrorPhoto*weightedErrorPhoto;
                            PhotoResidual += weightedErrorPhoto*weightedErrorPhoto;
                            ++nValidPhotoPts;
                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                float depth1 = transformedPoint3D(2);
                                float depthDiff = depth2 - depth1;
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*depth1;
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                float weightedErrorDepth = weight_depth * depthDiff;
                                //error2 += weightedErrorDepth*weightedErrorDepth;
                                DepthResidual += weightedErrorDepth*weightedErrorDepth;
                                ++nValidDepthPts;
                            }
                        }
                    }
                }
            }
        }
        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual,nValidPhotoPts,nValidDepthPts) //(+:error2)
#endif
//            for (int r=0;r<nRows;r++)
//            {
//                for (int c=0;c<nCols;c++)
//                {
//                    //Compute the 3D coordinates of the pij of the source frame
//                    Eigen::Vector3f point3D;
//                    point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
//    //            cout << "z " << depthSrcPyr[pyramidLevel].at<float>(r,c) << endl;
//                    if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
//                    {
//                        point3D(0)=(c - ox) * point3D(2) * inv_fx;
//                        point3D(1)=(r - oy) * point3D(2) * inv_fy;
//                        //Transform the 3D point using the transformation matrix Rt
//                        Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

            for (int i=0; i < LUT_xyz_sphere.size(); i++)
//            for (int r=0;r<nRows;r++)
//            {
//                for (int c=0;c<nCols;c++)
                {
                    //int i = r*nCols + c;
                    //std::cout << "LUT_xyz_sphere " << i << " xyz " << LUT_xyz_sphere[i].transpose() << std::endl;
                    // The depth is represented by the 'z' coordinate of LUT_xyz_sphere[i]
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                    {
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                // Apply M-estimator weighting
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                                //error2 += weightedErrorPhoto*weightedErrorPhoto;
                                PhotoResidual += weightedErrorPhoto*weightedErrorPhoto;
                                ++nValidPhotoPts;
//                            std::cout << "error2 " << error2 << " weightedErrorPhoto " << weightedErrorPhoto << " " << weight_photo << " " << photoDiff << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    //Obtain the depth values that will be used to the compute the depth residual
                                    float depth1 = transformedPoint3D(2);
                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    //error2 += weightedErrorDepth*weightedErrorDepth;
                                    DepthResidual += weightedErrorDepth*weightedErrorDepth;
                                    ++nValidDepthPts;
                                }
                            }
                        }
                    }
                }
            //}
        }

        //avPhotoResidual = sqrt(PhotoResidual / nValidPhotoPts);
        avPhotoResidual = sqrt(PhotoResidual / nValidDepthPts);
        avDepthResidual = sqrt(DepthResidual / nValidDepthPts);
        avResidual = avPhotoResidual + avDepthResidual;

        // std::cout << "PhotoResidual " << PhotoResidual << " DepthResidual " << DepthResidual << std::endl;
        // std::cout << "nValidPhotoPts " << nValidPhotoPts << " nValidDepthPts " << nValidDepthPts << std::endl;
        // std::cout << "avPhotoResidual " << avPhotoResidual << " avDepthResidual " << avDepthResidual << std::endl;

        return avResidual;
        //return error2;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    void calcHessGrad(const int &pyramidLevel,
                               const Eigen::Matrix4f poseGuess,
                               costFuncType method = PHOTO_CONSISTENCY)
    {
        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];

//        double varianceRegularization = 1; // 63%
//        double stdDevReg = sqrt(varianceRegularization);
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());
        }

//        if(bUseSalientPixels)
//        {
//#if ENABLE_OPENMP
//#pragma omp parallel for
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
//                int r = vSalientPixels[pyramidLevel][i] / nCols;
//                int c = vSalientPixels[pyramidLevel][i] % nCols;
////            cout << "vSalientPixels[pyramidLevel][i] " << vSalientPixels[pyramidLevel][i] << " " << r << " " << c << endl;

//                //Compute the 3D coordinates of the pij of the source frame
//                Eigen::Vector3f point3D;
//                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
////            cout << "z " << depthSrcPyr[pyramidLevel].at<float>(r,c) << endl;
//                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
//                {
//                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
//                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

//                    //Transform the 3D point using the transformation matrix Rt
//                    Eigen::Vector3f rotatedPoint3D = rotation*point3D;
//                    Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
//                    rotatedPoint3D = transformedPoint3D;
////                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

//                    //Project the 3D point to the 2D plane
//                    float inv_transformedPz = 1.0/transformedPoint3D(2);
//                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
//                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
//                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
//                    int transformed_r_int = round(transformed_r);
//                    int transformed_c_int = round(transformed_c);

//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
//                        (transformed_c_int>=0 && transformed_c_int < nCols) )
//                    {
//                        //Compute the pixel jacobian
//                        Eigen::Matrix<float,2,6> jacobianWarpRt;

//                        //Derivative with respect to x
//                        jacobianWarpRt(0,0)=fx*inv_transformedPz;
//                        jacobianWarpRt(1,0)=0;

//                        //Derivative with respect to y
//                        jacobianWarpRt(0,1)=0;
//                        jacobianWarpRt(1,1)=fy*inv_transformedPz;

//                        //Derivative with respect to z
//                        jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
//                        jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

//                        //Derivative with respect to \w_x
//                        jacobianWarpRt(0,3)=-fx*rotatedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
//                        jacobianWarpRt(1,3)=-fy*(rotatedPoint3D(2)+rotatedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz)*inv_transformedPz;

//                        //Derivative with respect to \w_y
//                        jacobianWarpRt(0,4)= fx*(rotatedPoint3D(2)+rotatedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz)*inv_transformedPz;
//                        jacobianWarpRt(1,4)= fy*rotatedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

//                        //Derivative with respect to \w_z
//                        jacobianWarpRt(0,5)=-fx*rotatedPoint3D(1)*inv_transformedPz;
//                        jacobianWarpRt(1,5)= fy*rotatedPoint3D(0)*inv_transformedPz;

////                        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
////                        float photoDiff2, depthDiff2;
//                        float pixel1, pixel2, depth1, depth2;
//                        float weightedErrorPhoto, weightedErrorDepth;
//                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
////                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
////                            if(photoDiff2 > varianceRegularization)
////                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
//                            weightedErrorPhoto = weight_photo * photoDiff;

//                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
//                            Eigen::Matrix<float,1,2> target_imgGradient;
//                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
//                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
////                        cout << "target_imgGradient " << target_imgGradient << endl;

//                            if(visualizeIterations)
//                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = pixel1;
//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the depth values that will be used to the compute the depth residual
//                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                weightedErrorDepth = weight_depth * depthDiff;

//                                //Depth jacobian:
//                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
//                                Eigen::Matrix<float,1,2> target_depthGradient;
//                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);

//                                Eigen::Matrix<float,1,6> jacobianRt_z;
//                                jacobianRt_z << 0,0,1,rotatedPoint3D(1),-rotatedPoint3D(0),0;
//                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
////                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;

//                                if(visualizeIterations)
//                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = depth1;
//                            }
//                        }

//                        //Assign the pixel residual and jacobian to its corresponding row
//#if ENABLE_OPENMP
//#pragma omp critical
//#endif
//                        {
//                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                            {
//                                // Photometric component
////                                hessian += jacobianPhoto.transpose()*jacobianPhoto / varPhoto;
//                                hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
//                            }
//                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                // Depth component (Plane ICL like)
//                                hessian += jacobianDepth.transpose()*jacobianDepth;
//                                gradient += jacobianDepth.transpose()*weightedErrorDepth;
//                            }
//                        }

//                    }
//                }
//            }
//        }
//        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for
#endif
            for (int i=0; i < LUT_xyz_sphere.size(); i++)
//            for (int r=0;r<nRows;r++)
//            {
//                for (int c=0;c<nCols;c++)
                {
                    //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                    {
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            //Compute the pixel jacobian
                            Eigen::Matrix<float,2,6> jacobianWarpRt;

                            //Derivative with respect to x
                            jacobianWarpRt(0,0)=fx*inv_transformedPz;
                            jacobianWarpRt(1,0)=0;

                            //Derivative with respect to y
                            jacobianWarpRt(0,1)=0;
                            jacobianWarpRt(1,1)=fy*inv_transformedPz;

                            //Derivative with respect to z
                            float inv_transformedPz_2 = inv_transformedPz*inv_transformedPz;
                            jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz_2;
                            jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz_2;

                            //Derivative with respect to \w_x
                            jacobianWarpRt(0,3)=-fx*transformedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz_2;
                            jacobianWarpRt(1,3)=-fy*(1+transformedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz_2);

                            //Derivative with respect to \w_y
                            jacobianWarpRt(0,4)= fx*(1+transformedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz_2);
                            jacobianWarpRt(1,4)= fy*transformedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz_2;

                            //Derivative with respect to \w_z
                            jacobianWarpRt(0,5)=-fx*transformedPoint3D(1)*inv_transformedPz;
                            jacobianWarpRt(1,5)= fy*transformedPoint3D(0)*inv_transformedPz;

    //                        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
    //                        float photoDiff2, depthDiff2;
                            float pixel1, pixel2, depth1, depth2;
                            float weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
    //                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
    //                            if(photoDiff2 > varianceRegularization)
    //                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                                weightedErrorPhoto = weight_photo * photoDiff;

                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
    //                        cout << "target_imgGradient " << target_imgGradient << endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = transformedPoint3D(2);

                                Eigen::Matrix<float,1,2> target_depthGradient;
                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                    continue;

                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    float depthDiff = depth2 - transformedPoint3D(2);
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*transformedPoint3D(2);
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;

                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,6> jacobianRt_z;
                                    jacobianRt_z << 0,0,1,transformedPoint3D(1),-transformedPoint3D(0),0;
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
    //                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;
                                }
                            }

                            //Assign the pixel residual and jacobian to its corresponding row
    #if ENABLE_OPENMP
    #pragma omp critical
    #endif
                            {
                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                {
                                    // Photometric component
    //                                hessian += jacobianPhoto.transpose()*jacobianPhoto / varPhoto;
                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
                                }
                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Depth component (Plane ICL like)
                                    hessian += jacobianDepth.transpose()*jacobianDepth;
                                    gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                }
                            }

                        }
                    }
                }
            //}
        }
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method. */
    double errorPhotoICP_Occ1(const int &pyramidLevel, const Eigen::Matrix4f poseGuess, costFuncType method = PHOTO_CONSISTENCY)
    {
        //double error2 = 0.0; // Squared error
        double PhotoResidual = 0.0;
        double DepthResidual = 0.0;
        int nValidPhotoPts = 0;
        int nValidDepthPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

//        float varianceRegularization = 1; // 63%
//        float stdDevReg = sqrt(varianceRegularization);
        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

//    std::cout << "poseGuess \n" << poseGuess << std::endl;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

//        if(bUseSalientPixels)
//        {

//#if ENABLE_OPENMP
//#pragma omp parallel for reduction (+:error2)
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
////                //                int i = nCols*r+c; //vector index
////                int r = vSalientPixels[pyramidLevel][i] / nCols;
////                int c = vSalientPixels[pyramidLevel][i] % nCols;

////                //Compute the 3D coordinates of the pij of the source frame
////                Eigen::Vector3f point3D;
////                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
////                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
////                {
////                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
////                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

////                    //Transform the 3D point using the transformation matrix Rt
////                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

//                if(LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]](0) != INVALID_POINT) //Compute the jacobian only for the valid points
//                {
//                    Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]] + translation;

//                    //Project the 3D point to the 2D plane
//                    float inv_transformedPz = 1.0/transformedPoint3D(2);
//                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
//                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
//                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
//                    int transformed_r_int = round(transformed_r);
//                    int transformed_c_int = round(transformed_c);

//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
//                        (transformed_c_int>=0 && transformed_c_int < nCols) )
//                    {
//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            // float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            float pixel1 = graySrcPyr[pyramidLevel].data[vSalientPixels[pyramidLevel][i]]; // Intensity value of the pixel(r,c) of source frame
//                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
//                            float weightedErrorPhoto = weight_photo * photoDiff;
//                            // Apply M-estimator weighting
////                            if(photoDiff2 > varianceRegularization)
////                            {
//////                                float photoDiff2_norm = sqrt(photoDiff2);
//////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
////                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
////                            }
//                            error2 += weightedErrorPhoto*weightedErrorPhoto;

//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                //Obtain the depth values that will be used to the compute the depth residual
//                                float depth1 = transformedPoint3D(2);
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                float weightedErrorDepth = weight_depth * depthDiff;
//                                error2 += weightedErrorDepth*weightedErrorDepth;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:nValidPhotoPts,nValidDepthPts) // nValidPhotoPts,  //for reduction (+:error2)
#endif
            for (int i=0; i < LUT_xyz_sphere.size(); i++)
                {
                    //int i = r*nCols + c;
                    // The depth is represented by the 'z' coordinate of LUT_xyz_sphere[i]
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                    {
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            // Discard occluded points
                            int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                            if(invDepthBuffer(ii) > 0 && inv_transformedPz < invDepthBuffer(ii)) // the current pixel is occluded
                                continue;
                            invDepthBuffer(ii) = inv_transformedPz;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(fabs(grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity &&
                                   fabs(grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                //float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                // Apply M-estimator weighting
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                                residualsPhoto(ii) = weightedErrorPhoto*weightedErrorPhoto;
                                ++nValidPhotoPts;
                                // error2 += weightedErrorPhoto*weightedErrorPhoto;
//                            std::cout << "error2 " << error2 << " weightedErrorPhoto " << weightedErrorPhoto << " " << weight_photo << " " << photoDiff << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if( fabs(depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth &&
                                        fabs(depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth)
                                        continue;

                                    //Obtain the depth values that will be used to the compute the depth residual
                                    float depth1 = transformedPoint3D(2);
                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    //error2 += weightedErrorDepth*weightedErrorDepth;
                                    residualsDepth(ii) = weightedErrorDepth*weightedErrorDepth;
                                    ++nValidDepthPts;
                                }
                            }
                        }
                    }
                }
            //}
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual) // nValidPhotoPts, nValidDepthPts
#endif
            for(int i=0; i < imgSize; i++)
            {
                PhotoResidual += residualsPhoto(i);
                DepthResidual += residualsDepth(i);
//                if(residualsPhoto(i) > 0) ++nValidPhotoPts;
//                if(residualsDepth(i) > 0) ++nValidDepthPts;
            }
        }

        avPhotoResidual = sqrt(PhotoResidual / nValidPhotoPts);
        //avPhotoResidual = sqrt(PhotoResidual / nValidDepthPts);
        avDepthResidual = sqrt(DepthResidual / nValidDepthPts);
        avResidual = avPhotoResidual + avDepthResidual;

        // std::cout << "PhotoResidual " << PhotoResidual << " DepthResidual " << DepthResidual << std::endl;
        // std::cout << "nValidPhotoPts " << nValidPhotoPts << " nValidDepthPts " << nValidDepthPts << std::endl;
        // std::cout << "avPhotoResidual " << avPhotoResidual << " avDepthResidual " << avDepthResidual << std::endl;

        return avResidual;
        //return error2;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient. */
    void calcHessGrad_Occ1(const int &pyramidLevel,
                           const Eigen::Matrix4f poseGuess,
                           costFuncType method = PHOTO_CONSISTENCY)
    {
        int nRows = graySrcPyr[pyramidLevel].rows;
        int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

        Eigen::MatrixXf jacobiansPhoto = Eigen::MatrixXf::Zero(imgSize,6);
        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::MatrixXf jacobiansDepth = Eigen::MatrixXf::Zero(imgSize,6);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];

//        double varianceRegularization = 1; // 63%
//        double stdDevReg = sqrt(varianceRegularization);
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());

//            // Initialize the mask to segment the dynamic objects and the occlusions
//            mask_dynamic_occlusion = cv::Mat::zeros(nRows, nCols, CV_8U);
        }

        Eigen::VectorXf correspTrgSrc = Eigen::VectorXf::Zero(imgSize);
        //std::vector<float> depthDiff_(imgSize,-1);
        int numVisiblePixels = 0;

#if ENABLE_OPENMP
#pragma omp parallel for reduction(+:numVisiblePixels)
#endif
        for (int i=0; i < LUT_xyz_sphere.size(); i++)
//            for (int r=0;r<nRows;r++)
//            {
//                for (int c=0;c<nCols;c++)
            {
                //int i = r*nCols + c;
                if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                {
                    Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                    // Project the 3D point to the 2D plane
                    float inv_transformedPz = 1.0/transformedPoint3D(2);
                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);

                    // Asign the intensity value to the warped image and compute the difference between the transformed
                    // pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        // Discard occluded points
                        int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                        if(invDepthBuffer(ii) == 0)
                            ++numVisiblePixels;
                            else
                            {
                                if(inv_transformedPz < invDepthBuffer(ii)) // the current pixel is occluded
                                {
//                                    mask_dynamic_occlusion.at<uchar>(i) = 55;
                                    continue;
                                }
//                                else // The previous pixel used was occluded
//                                    mask_dynamic_occlusion.at<uchar>(correspTrgSrc[ii]) = 55;
                            }

                        ++numVisiblePixels;
                        invDepthBuffer(ii) = inv_transformedPz;
                        //correspTrgSrc(ii) = i;

                        // Compute the pixel jacobian
                        Eigen::Matrix<float,2,6> jacobianWarpRt;

                        // Derivative with respect to x
                        jacobianWarpRt(0,0)=fx*inv_transformedPz;
                        jacobianWarpRt(1,0)=0;

                        // Derivative with respect to y
                        jacobianWarpRt(0,1)=0;
                        jacobianWarpRt(1,1)=fy*inv_transformedPz;

                        // Derivative with respect to z
                        float inv_transformedPz_2 = inv_transformedPz*inv_transformedPz;
                        jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz_2;
                        jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz_2;

                        // Derivative with respect to \w_x
                        jacobianWarpRt(0,3)=-fx*transformedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz_2;
                        jacobianWarpRt(1,3)=-fy*(1+transformedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz_2);

                        // Derivative with respect to \w_y
                        jacobianWarpRt(0,4)= fx*(1+transformedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz_2);
                        jacobianWarpRt(1,4)= fy*transformedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz_2;

                        // Derivative with respect to \w_z
                        jacobianWarpRt(0,5)=-fx*transformedPoint3D(1)*inv_transformedPz;
                        jacobianWarpRt(1,5)= fy*transformedPoint3D(0)*inv_transformedPz;

                        float pixel1, pixel2, depth1, depth2;
                        float weightedErrorPhoto, weightedErrorDepth;
                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            if(visualizeIterations)
                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                            Eigen::Matrix<float,1,2> target_imgGradient;
                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                continue;

                            //Obtain the pixel values that will be used to compute the pixel residual
                            pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
//                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
//                            if(photoDiff2 > varianceRegularization)
//                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                            weightedErrorPhoto = weight_photo * photoDiff;

                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
//                        cout << "target_imgGradient " << target_imgGradient << endl;
                            //cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << endl;

                            jacobiansPhoto.block(i,0,1,6) = jacobianPhoto;
                            residualsPhoto(i) = weightedErrorPhoto;
                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            if(visualizeIterations)
                                warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = transformedPoint3D(2);

                            Eigen::Matrix<float,1,2> target_depthGradient;
                            target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                continue;

                            //Obtain the depth values that will be used to the compute the depth residual
                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                float depthDiff = depth2 - transformedPoint3D(2);
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*transformedPoint3D(2);
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                weightedErrorDepth = weight_depth * depthDiff;

                                //Depth jacobian:
                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                Eigen::Matrix<float,1,6> jacobianRt_z;
                                jacobianRt_z << 0,0,1,transformedPoint3D(1),-transformedPoint3D(0),0;
                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
//                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;

                                jacobiansDepth.block(i,0,1,6) = jacobianDepth;
                                residualsDepth(i) = weightedErrorDepth;
                            }
                        }
                    }
                }
            }
        //}

//        std::cout << "jacobiansPhoto \n" << jacobiansPhoto << std::endl;
//        std::cout << "residualsPhoto \n" << residualsPhoto.transpose() << std::endl;
//        std::cout << "jacobiansDepth \n" << jacobiansDepth << std::endl;
//        std::cout << "residualsDepth \n" << residualsDepth.transpose() << std::endl;

//#if ENABLE_OPENMP
//#pragma omp parallel for reduction (+:hessian,gradient) // Cannot reduce on Eigen types
//#endif
        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
            for (int i=0; i<imgSize; i++)
                if(residualsPhoto(i) != 0)
                {
                    hessian += jacobiansPhoto.block(i,0,1,6).transpose() * jacobiansPhoto.block(i,0,1,6);
                    gradient += jacobiansPhoto.block(i,0,1,6).transpose() * residualsPhoto(i);
                }
        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
            for (int i=0; i<imgSize; i++)
                if(residualsPhoto(i) != 0)
                {
                    hessian += jacobiansDepth.block(i,0,1,6).transpose() * jacobiansDepth.block(i,0,1,6);
                    gradient += jacobiansDepth.block(i,0,1,6).transpose() * residualsDepth(i);
                }
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method. */
    double errorPhotoICP_Occ2(const int &pyramidLevel, const Eigen::Matrix4f poseGuess, costFuncType method = PHOTO_CONSISTENCY)
    {
        //double error2 = 0.0; // Squared error
        double PhotoResidual = 0.0;
        double DepthResidual = 0.0;
        int nValidPhotoPts = 0;
        int nValidDepthPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

//        float varianceRegularization = 1; // 63%
//        float stdDevReg = sqrt(varianceRegularization);
        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

//    std::cout << "poseGuess \n" << poseGuess << std::endl;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

//        if(bUseSalientPixels)
//        {

//#if ENABLE_OPENMP
//#pragma omp parallel for reduction (+:error2)
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
////                //                int i = nCols*r+c; //vector index
////                int r = vSalientPixels[pyramidLevel][i] / nCols;
////                int c = vSalientPixels[pyramidLevel][i] % nCols;

////                //Compute the 3D coordinates of the pij of the source frame
////                Eigen::Vector3f point3D;
////                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
////                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
////                {
////                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
////                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

////                    //Transform the 3D point using the transformation matrix Rt
////                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

//                if(LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]](0) != INVALID_POINT) //Compute the jacobian only for the valid points
//                {
//                    Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[vSalientPixels[pyramidLevel][i]] + translation;

//                    //Project the 3D point to the 2D plane
//                    float inv_transformedPz = 1.0/transformedPoint3D(2);
//                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
//                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
//                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
//                    int transformed_r_int = round(transformed_r);
//                    int transformed_c_int = round(transformed_c);

//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
//                        (transformed_c_int>=0 && transformed_c_int < nCols) )
//                    {
//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            // float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            float pixel1 = graySrcPyr[pyramidLevel].data[vSalientPixels[pyramidLevel][i]]; // Intensity value of the pixel(r,c) of source frame
//                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
//                            float weightedErrorPhoto = weight_photo * photoDiff;
//                            // Apply M-estimator weighting
////                            if(photoDiff2 > varianceRegularization)
////                            {
//////                                float photoDiff2_norm = sqrt(photoDiff2);
//////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
////                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
////                            }
//                            error2 += weightedErrorPhoto*weightedErrorPhoto;

//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                //Obtain the depth values that will be used to the compute the depth residual
//                                float depth1 = transformedPoint3D(2);
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                float weightedErrorDepth = weight_depth * depthDiff;
//                                error2 += weightedErrorDepth*weightedErrorDepth;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:nValidPhotoPts,nValidDepthPts) // nValidPhotoPts,  //for reduction (+:error2)
#endif
            for (int i=0; i < LUT_xyz_sphere.size(); i++)
                {
                    //int i = r*nCols + c;
                    // The depth is represented by the 'z' coordinate of LUT_xyz_sphere[i]
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                    {
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            // Discard outliers (occluded and moving points)
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            float depthDiff = depth2 - inv_transformedPz;
                            if(fabs(depthDiff) > thresDepthOutliers)
                                continue;

                            // Discard occluded points
                            int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                            if(invDepthBuffer(ii) > 0 && inv_transformedPz < invDepthBuffer(ii)) // the current pixel is occluded
                                continue;
                            invDepthBuffer(ii) = inv_transformedPz;


//                            // Discard outlies: both from occlusions and moving objects
//                            if( fabs() )

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                 if(fabs(grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity &&
                                    fabs(grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity)
                                     continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                //float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                // Apply M-estimator weighting
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                                residualsPhoto(ii) = weightedErrorPhoto*weightedErrorPhoto;
                                ++nValidPhotoPts;
                                // error2 += weightedErrorPhoto*weightedErrorPhoto;
//                            std::cout << "error2 " << error2 << " weightedErrorPhoto " << weightedErrorPhoto << " " << weight_photo << " " << photoDiff << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if( fabs(depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth &&
                                        fabs(depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth)
                                        continue;

                                    //Obtain the depth values that will be used to the compute the depth residual
                                    float depth1 = transformedPoint3D(2);
                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    //error2 += weightedErrorDepth*weightedErrorDepth;
                                    residualsDepth(ii) = weightedErrorDepth*weightedErrorDepth;
                                    ++nValidDepthPts;
                                }
                            }
                        }
                    }
                }
            //}
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual) // nValidPhotoPts, nValidDepthPts
#endif
            for(int i=0; i < imgSize; i++)
            {
                PhotoResidual += residualsPhoto(i);
                DepthResidual += residualsDepth(i);
//                if(residualsPhoto(i) > 0) ++nValidPhotoPts;
//                if(residualsDepth(i) > 0) ++nValidDepthPts;
            }
        }

        avPhotoResidual = sqrt(PhotoResidual / nValidPhotoPts);
        //avPhotoResidual = sqrt(PhotoResidual / nValidDepthPts);
        avDepthResidual = sqrt(DepthResidual / nValidDepthPts);
        avResidual = avPhotoResidual + avDepthResidual;

        // std::cout << "PhotoResidual " << PhotoResidual << " DepthResidual " << DepthResidual << std::endl;
        // std::cout << "nValidPhotoPts " << nValidPhotoPts << " nValidDepthPts " << nValidDepthPts << std::endl;
        // std::cout << "avPhotoResidual " << avPhotoResidual << " avDepthResidual " << avDepthResidual << std::endl;

        return avResidual;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient. */
    void calcHessGrad_Occ2(const int &pyramidLevel,
                           const Eigen::Matrix4f poseGuess,
                           costFuncType method = PHOTO_CONSISTENCY)
    {
        int nRows = graySrcPyr[pyramidLevel].rows;
        int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

        Eigen::MatrixXf jacobiansPhoto = Eigen::MatrixXf::Zero(imgSize,6);
        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::MatrixXf jacobiansDepth = Eigen::MatrixXf::Zero(imgSize,6);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];

//        double varianceRegularization = 1; // 63%
//        double stdDevReg = sqrt(varianceRegularization);
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());

            // Initialize the mask to segment the dynamic objects and the occlusions
            mask_dynamic_occlusion = cv::Mat::zeros(nRows, nCols, CV_8U);
        }

        Eigen::VectorXf correspTrgSrc = Eigen::VectorXf::Zero(imgSize);
        std::vector<float> depthDiff_(imgSize,-1);
        int numVisiblePixels = 0;
        float pixel1, pixel2, depth1, depth2;

#if ENABLE_OPENMP
#pragma omp parallel for reduction(+:numVisiblePixels)
#endif
        for (int i=0; i < LUT_xyz_sphere.size(); i++)
//            for (int r=0;r<nRows;r++)
//            {
//                for (int c=0;c<nCols;c++)
            {
                //int i = r*nCols + c;
                if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                {
                    Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;

                    // Project the 3D point to the 2D plane
                    float inv_transformedPz = 1.0/transformedPoint3D(2);
                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);

                    // Asign the intensity value to the warped image and compute the difference between the transformed
                    // pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        // Discard outliers (occluded and moving points)
                        //float dist_src = transformedPoint3D.norm();
                        depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                        float depthDiff = depth2 - transformedPoint3D(2);
                        if(fabs(depthDiff) > thresDepthOutliers)
                        {
//                            if(visualizeIterations)
//                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                                {
//                                    if(depthDiff > 0)
//                                        mask_dynamic_occlusion.at<uchar>(i) = 255;
//                                    else
//                                        mask_dynamic_occlusion.at<uchar>(i) = 155;
//                                }
                            //assert(false);
                            continue;
                        }

                        // Discard occluded points
                        int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                        if(invDepthBuffer(ii) == 0)
                            ++numVisiblePixels;
                            else
                            {
                                if(inv_transformedPz < invDepthBuffer(ii)) // the current pixel is occluded
                                {
                                    mask_dynamic_occlusion.at<uchar>(i) = 55;
                                    continue;
                                }
                                else // The previous pixel used was occluded
                                    mask_dynamic_occlusion.at<uchar>(correspTrgSrc[ii]) = 55;
                            }

                        ++numVisiblePixels;
                        invDepthBuffer(ii) = inv_transformedPz;
                        correspTrgSrc(ii) = i;

                        // Compute the pixel jacobian
                        Eigen::Matrix<float,2,6> jacobianWarpRt;

                        // Derivative with respect to x
                        jacobianWarpRt(0,0)=fx*inv_transformedPz;
                        jacobianWarpRt(1,0)=0;

                        // Derivative with respect to y
                        jacobianWarpRt(0,1)=0;
                        jacobianWarpRt(1,1)=fy*inv_transformedPz;

                        // Derivative with respect to z
                        float inv_transformedPz_2 = inv_transformedPz*inv_transformedPz;
                        jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz_2;
                        jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz_2;

                        // Derivative with respect to \w_x
                        jacobianWarpRt(0,3)=-fx*transformedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz_2;
                        jacobianWarpRt(1,3)=-fy*(1+transformedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz_2);

                        // Derivative with respect to \w_y
                        jacobianWarpRt(0,4)= fx*(1+transformedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz_2);
                        jacobianWarpRt(1,4)= fy*transformedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz_2;

                        // Derivative with respect to \w_z
                        jacobianWarpRt(0,5)=-fx*transformedPoint3D(1)*inv_transformedPz;
                        jacobianWarpRt(1,5)= fy*transformedPoint3D(0)*inv_transformedPz;

                        float weightedErrorPhoto, weightedErrorDepth;
                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            if(visualizeIterations)
                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                            Eigen::Matrix<float,1,2> target_imgGradient;
                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                continue;

                            //Obtain the pixel values that will be used to compute the pixel residual
                            pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
//                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
//                            if(photoDiff2 > varianceRegularization)
//                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                            weightedErrorPhoto = weight_photo * photoDiff;

                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
//                        cout << "target_imgGradient " << target_imgGradient << endl;

                            jacobiansPhoto.block(i,0,1,6) = jacobianPhoto;
                            residualsPhoto(i) = weightedErrorPhoto;
                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            if(visualizeIterations)
                                warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = transformedPoint3D(2);

                            Eigen::Matrix<float,1,2> target_depthGradient;
                            target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                continue;

                            //Obtain the depth values that will be used to the compute the depth residual
                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                float depthDiff = depth2 - transformedPoint3D(2);
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*transformedPoint3D(2);
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                weightedErrorDepth = weight_depth * depthDiff;

                                //Depth jacobian:
                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                Eigen::Matrix<float,1,6> jacobianRt_z;
                                jacobianRt_z << 0,0,1,transformedPoint3D(1),-transformedPoint3D(0),0;
                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
//                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;

                                jacobiansDepth.block(i,0,1,6) = jacobianDepth;
                                residualsDepth(i) = weightedErrorDepth;
                                depthDiff_[ii] = fabs(depthDiff);
                            }
                            else
                                assert(false);
                        }
                    }
                }
            }
        //}
//#if ENABLE_OPENMP
//#pragma omp parallel for reduction (+:hessian,gradient) // Cannot reduce on Eigen types
//#endif
        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
            for (int i=0; i<imgSize; i++)
                if(residualsPhoto(i) != 0)
                {
                    hessian += jacobiansPhoto.block(i,0,1,6).transpose() * jacobiansPhoto.block(i,0,1,6);
                    gradient += jacobiansPhoto.block(i,0,1,6).transpose() * residualsPhoto(i);
                }
        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
            for (int i=0; i<imgSize; i++)
                if(residualsPhoto(i) != 0)
                {
                    hessian += jacobiansDepth.block(i,0,1,6).transpose() * jacobiansDepth.block(i,0,1,6);
                    gradient += jacobiansDepth.block(i,0,1,6).transpose() * residualsDepth(i);
                }

        SSO = (float)numVisiblePixels / imgSize;
//        std::cout << "numVisiblePixels " << numVisiblePixels << " imgSize " << imgSize << " sso " << SSO << std::endl;

        std::vector<float> diffDepth(imgSize);
        int validPt = 0;
        for(int i=0; i < imgSize; i++)
            if(depthDiff_[i] >= 0)
                diffDepth[validPt++] = depthDiff_[i];
        float diffDepthMean, diffDepthStDev;
        calcMeanAndStDev(diffDepth, diffDepthMean, diffDepthStDev);
        std::cout << "diffDepthMean " << diffDepthMean << " diffDepthStDev " << diffDepthStDev << " trans " << poseGuess.block(0,3,3,1).norm() << " sso " << SSO << std::endl;
    }


    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    double calcPhotoICP_SqError(const int &pyramidLevel, const Eigen::Matrix4f poseGuess, double varPhoto = pow(3./255,2), double varDepth = 0.0001, costFuncType method = PHOTO_CONSISTENCY)
    {
        double error2 = 0.0; // Squared error

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

//        float varianceRegularization = 1; // 63%
//        float stdDevReg = sqrt(varianceRegularization);
        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

//    std::cout << "poseGuess \n" << poseGuess << std::endl;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(bUseSalientPixels)
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:error2)
#endif
            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
            {
                //                int i = nCols*r+c; //vector index
                int r = vSalientPixels[pyramidLevel][i] / nCols;
                int c = vSalientPixels[pyramidLevel][i] % nCols;

                //Compute the 3D coordinates of the pij of the source frame
                Eigen::Vector3f point3D;
                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                {
                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

                    //Transform the 3D point using the transformation matrix Rt
                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

                    //Project the 3D point to the 2D plane
                    float inv_transformedPz = 1.0/transformedPoint3D(2);
                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);

                    //Asign the intensity value to the warped image and compute the difference between the transformed
                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            //Obtain the pixel values that will be used to compute the pixel residual
                            float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                            float weightedErrorPhoto = weight_photo * photoDiff;
                            // Apply M-estimator weighting
//                            if(photoDiff2 > varianceRegularization)
//                            {
////                                float photoDiff2_norm = sqrt(photoDiff2);
////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
//                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
//                            }
                            error2 += weightedErrorPhoto*weightedErrorPhoto;

                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                float depthDiff = depth2 - depth1;
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*depth1;
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                float weightedErrorDepth = weight_depth * depthDiff;
                                error2 += weightedErrorDepth*weightedErrorDepth;
                            }
                        }
                    }
                }
            }
        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:error2)
#endif
            for (int r=0;r<nRows;r++)
            {
                for (int c=0;c<nCols;c++)
                {
                    //                int i = nCols*r+c; //vector index

                    //Compute the 3D coordinates of the pij of the source frame
                    Eigen::Vector3f point3D;
                    point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                    {
                        point3D(0)=(c - ox) * point3D(2) * inv_fx;
                        point3D(1)=(r - oy) * point3D(2) * inv_fy;

                        //Transform the 3D point using the transformation matrix Rt
                        Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                // Apply M-estimator weighting
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                                error2 += weightedErrorPhoto*weightedErrorPhoto;
//                            std::cout << "error2 " << error2 << " weightedErrorPhoto " << weightedErrorPhoto << " " << weight_photo << " " << photoDiff << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    //Obtain the depth values that will be used to the compute the depth residual
                                    float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    error2 += weightedErrorDepth*weightedErrorDepth;
                                }
                            }
                        }
                    }
                }
            }
        }

        return error2;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    void calcHessianAndGradient(const int &pyramidLevel,
                               const Eigen::Matrix4f poseGuess,
                               costFuncType method = PHOTO_CONSISTENCY)
    {
        int nRows = graySrcPyr[pyramidLevel].rows;
        int nCols = graySrcPyr[pyramidLevel].cols;

        float scaleFactor = 1.0/pow(2,pyramidLevel);
        float fx = cameraMatrix(0,0)*scaleFactor;
        float fy = cameraMatrix(1,1)*scaleFactor;
        float ox = cameraMatrix(0,2)*scaleFactor;
        float oy = cameraMatrix(1,2)*scaleFactor;
        float inv_fx = 1./fx;
        float inv_fy = 1./fy;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];

//        double varianceRegularization = 1; // 63%
//        double stdDevReg = sqrt(varianceRegularization);
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());
        }

//        if(bUseSalientPixels)
//        {
//#if ENABLE_OPENMP
//#pragma omp parallel for
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
//                int r = vSalientPixels[pyramidLevel][i] / nCols;
//                int c = vSalientPixels[pyramidLevel][i] % nCols;
////            cout << "vSalientPixels[pyramidLevel][i] " << vSalientPixels[pyramidLevel][i] << " " << r << " " << c << endl;

//                //Compute the 3D coordinates of the pij of the source frame
//                Eigen::Vector3f point3D;
//                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
////            cout << "z " << depthSrcPyr[pyramidLevel].at<float>(r,c) << endl;
//                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
//                {
//                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
//                    point3D(1)=(r - oy) * point3D(2) * inv_fy;

//                    //Transform the 3D point using the transformation matrix Rt
//                    Eigen::Vector3f rotatedPoint3D = rotation*point3D;
//                    Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
//                    rotatedPoint3D = transformedPoint3D;
////                    Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

//                    //Project the 3D point to the 2D plane
//                    float inv_transformedPz = 1.0/transformedPoint3D(2);
//                    float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
//                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
//                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
//                    int transformed_r_int = round(transformed_r);
//                    int transformed_c_int = round(transformed_c);

//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
//                        (transformed_c_int>=0 && transformed_c_int < nCols) )
//                    {
//                        //Compute the pixel jacobian
//                        Eigen::Matrix<float,2,6> jacobianWarpRt;

//                        //Derivative with respect to x
//                        jacobianWarpRt(0,0)=fx*inv_transformedPz;
//                        jacobianWarpRt(1,0)=0;

//                        //Derivative with respect to y
//                        jacobianWarpRt(0,1)=0;
//                        jacobianWarpRt(1,1)=fy*inv_transformedPz;

//                        //Derivative with respect to z
//                        jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
//                        jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

//                        //Derivative with respect to \lambda_x
//                        jacobianWarpRt(0,3)=-fx*rotatedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
//                        jacobianWarpRt(1,3)=-fy*(rotatedPoint3D(2)+rotatedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz)*inv_transformedPz;

//                        //Derivative with respect to \lambda_y
//                        jacobianWarpRt(0,4)= fx*(rotatedPoint3D(2)+rotatedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz)*inv_transformedPz;
//                        jacobianWarpRt(1,4)= fy*rotatedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

//                        //Derivative with respect to \lambda_z
//                        jacobianWarpRt(0,5)=-fx*rotatedPoint3D(1)*inv_transformedPz;
//                        jacobianWarpRt(1,5)= fy*rotatedPoint3D(0)*inv_transformedPz;

////                        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
////                        float photoDiff2, depthDiff2;
//                        float pixel1, pixel2, depth1, depth2;
//                        float weightedErrorPhoto, weightedErrorDepth;
//                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
////                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
////                            if(photoDiff2 > varianceRegularization)
////                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
//                            weightedErrorPhoto = weight_photo * photoDiff;

//                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
//                            Eigen::Matrix<float,1,2> target_imgGradient;
//                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
//                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
////                        cout << "target_imgGradient " << target_imgGradient << endl;

//                            if(visualizeIterations)
//                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = pixel1;
//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the depth values that will be used to the compute the depth residual
//                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                weightedErrorDepth = weight_depth * depthDiff;

//                                //Depth jacobian:
//                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
//                                Eigen::Matrix<float,1,2> target_depthGradient;
//                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);

//                                Eigen::Matrix<float,1,6> jacobianRt_z;
//                                jacobianRt_z << 0,0,1,rotatedPoint3D(1),-rotatedPoint3D(0),0;
//                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
////                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;

//                                if(visualizeIterations)
//                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = depth1;
//                            }
//                        }

//                        //Assign the pixel residual and jacobian to its corresponding row
//#if ENABLE_OPENMP
//#pragma omp critical
//#endif
//                        {
//                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                            {
//                                // Photometric component
////                                hessian += jacobianPhoto.transpose()*jacobianPhoto / varPhoto;
//                                hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
//                            }
//                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                // Depth component (Plane ICL like)
//                                hessian += jacobianDepth.transpose()*jacobianDepth;
//                                gradient += jacobianDepth.transpose()*weightedErrorDepth;
//                            }
//                        }

//                    }
//                }
//            }
//        }
//        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for
#endif
            for (int r=0;r<nRows;r++)
            {
                for (int c=0;c<nCols;c++)
                {
                    //Compute the 3D coordinates of the pij of the source frame
                    Eigen::Vector3f point3D;
                    point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
    //            cout << "z " << depthSrcPyr[pyramidLevel].at<float>(r,c) << endl;
                    if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                    {
                        point3D(0)=(c - ox) * point3D(2) * inv_fx;
                        point3D(1)=(r - oy) * point3D(2) * inv_fy;

                        //Transform the 3D point using the transformation matrix Rt
    //                    Eigen::Vector3f rotatedPoint3D = rotation*point3D;
    //                    Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
                        Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;

                        //Project the 3D point to the 2D plane
                        float inv_transformedPz = 1.0/transformedPoint3D(2);
                        float transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            //Compute the pixel jacobian
                            Eigen::Matrix<float,2,6> jacobianWarpRt;

                            //Derivative with respect to x
                            jacobianWarpRt(0,0)=fx*inv_transformedPz;
                            jacobianWarpRt(1,0)=0;

                            //Derivative with respect to y
                            jacobianWarpRt(0,1)=0;
                            jacobianWarpRt(1,1)=fy*inv_transformedPz;

                            //Derivative with respect to z
                            float inv_transformedPz_2 = inv_transformedPz*inv_transformedPz;
                            jacobianWarpRt(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz_2;
                            jacobianWarpRt(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz_2;

                            //Derivative with respect to \lambda_x
                            jacobianWarpRt(0,3)=-fx*transformedPoint3D(1)*transformedPoint3D(0)*inv_transformedPz_2;
                            jacobianWarpRt(1,3)=-fy*(1+transformedPoint3D(1)*transformedPoint3D(1)*inv_transformedPz_2);

                            //Derivative with respect to \lambda_y
                            jacobianWarpRt(0,4)= fx*(1+transformedPoint3D(0)*transformedPoint3D(0)*inv_transformedPz_2);
                            jacobianWarpRt(1,4)= fy*transformedPoint3D(0)*transformedPoint3D(1)*inv_transformedPz_2;

                            //Derivative with respect to \lambda_z
                            jacobianWarpRt(0,5)=-fx*transformedPoint3D(1)*inv_transformedPz;
                            jacobianWarpRt(1,5)= fy*transformedPoint3D(0)*inv_transformedPz;

    //                        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
    //                        float photoDiff2, depthDiff2;
                            float pixel1, pixel2, depth1, depth2;
                            float weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(r,c);

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
    //                        cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << endl;
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
    //                            if(photoDiff2 > varianceRegularization)
    //                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                                weightedErrorPhoto = weight_photo * photoDiff;

                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
    //                        cout << "target_imgGradient " << target_imgGradient << endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = transformedPoint3D(2);

                                Eigen::Matrix<float,1,2> target_depthGradient;
                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                    continue;

                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    float depthDiff = depth2 - transformedPoint3D(2);
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*transformedPoint3D(2);
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;

                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,6> jacobianRt_z;
                                    jacobianRt_z << 0,0,1,transformedPoint3D(1),-transformedPoint3D(0),0;
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
    //                            cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;
                                }
                            }

                            //Assign the pixel residual and jacobian to its corresponding row
    #if ENABLE_OPENMP
    #pragma omp critical
    #endif
                            {
                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                {
                                    // Photometric component
    //                                hessian += jacobianPhoto.transpose()*jacobianPhoto / varPhoto;
                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
                                }
                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Depth component (Plane ICL like)
                                    hessian += jacobianDepth.transpose()*jacobianDepth;
                                    gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                }
                            }

                        }
                    }
                }
            }
        }
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
    This is done following the work in:
    Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
    in Computer Vision Workshops (ICCV Workshops), 2011. */
    double errorPhotoICP_sphere(const int &pyramidLevel,
                                const Eigen::Matrix4f &poseGuess, // The relative pose of the robot between the two frames
                                costFuncType method = PHOTO_CONSISTENCY )
    {
        double error2 = 0.0;
        int numValidPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        // depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        double stdDevPhoto_inv = 1./stdDevPhoto;
        double stdDevDepth_inv = 1./stdDevDepth;

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

//        if(bUseSalientPixels)
//        {
//#if ENABLE_OPENMP
//#pragma omp parallel for reduction (+:error2)
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
//                int r = vSalientPixels[pyramidLevel][i] / nCols;
//                int c = vSalientPixels[pyramidLevel][i] % nCols;
////            cout << "vSalientPixels[pyramidLevel][i] " << vSalientPixels[pyramidLevel][i] << " " << r << " " << c << endl;

//                float phi = (half_nRows-r)*angle_res;
//                float theta = c*angle_res;

//                //Compute the 3D coordinates of the pij of the source frame
//                Eigen::Vector3f point3D;
//                float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
////            std::cout << " d " << depth;
//                if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points

//                {
//                    point3D(0) = depth1*sin(phi);
//                    point3D(1) = -depth1*cos(phi)*sin(theta);
//                    point3D(2) = -depth1*cos(phi)*cos(theta);

//                    //Transform the 3D point using the transformation matrix Rt
//                    Eigen::Vector3f rotatedPoint3D = rotation*point3D;
//                    Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
////                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

//                    //Project the 3D point to the S2 sphere
//                    float dist_inv = 1.f / transformedPoint3D.norm();
//                    float phi_trg = asin(transformedPoint3D(0)*dist_inv);
//                    float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
//                    int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
//                    int transformed_c_int = round(theta_trg*angle_res_inv);
////                cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
//                    {
//                        assert(transformed_c_int >= 0 && transformed_c_int < nCols);

//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
//                            float weightedErrorPhoto = weight_photo * photoDiff;
////                            if(photoDiff2 > varianceRegularization)
////                            {
//////                                float photoDiff2_norm = sqrt(photoDiff2);
//////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
////                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
////                            }
//                            error2 += weightedErrorPhoto*weightedErrorPhoto;
//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                //Obtain the depth values that will be used to the compute the depth residual
//                                float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                float weightedErrorDepth = weight_depth * depthDiff;
//                                error2 += weightedErrorDepth*weightedErrorDepth;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//        else
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:error2,numValidPts)
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                // float phi = (half_nRows-r)*angle_res;
//                for(int c=0;c<nCols;c++)
//                    // {
//                    // float theta = c*angle_res;
//                    // int size_img = nRows*nCols;
                for(int i=0; i < LUT_xyz_sphere.size(); i++)
                {
                    //Compute the 3D coordinates of the pij of the source frame
                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    // std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                    //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                        // if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
                    {
                        //Eigen::Vector3f point3D = LUT_xyz_sphere[i];
                        // point3D(0) = depth1*sin(phi);
                        // point3D(1) = -depth1*cos(phi)*sin(theta);
                        // point3D(2) = -depth1*cos(phi)*cos(theta);
                        //Transform the 3D point using the transformation matrix Rt
                        //Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;
                        // cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;
                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
                        // cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
                            // cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << " " << nRows << "x" << nCols << endl;
                            assert(transformed_c_int >= 0 && transformed_c_int < nCols);
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(fabs(grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity &&
                                   fabs(grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                //float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                // if(photoDiff2 > varianceRegularization)
                                // {
                                //// float photoDiff2_norm = sqrt(photoDiff2);
                                //// double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
                                // photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
                                // }
                                error2 += weightedErrorPhoto*weightedErrorPhoto;
                                //cout << "photo err " << weightedErrorPhoto << endl;
                                ++numValidPts;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if( fabs(depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth &&
                                        fabs(depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth)
                                        continue;

                                    //Obtain the depth values that will be used to the compute the depth residual
                                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                    float depthDiff = depth2 - dist;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth2; // We assume that: depth1 ~ depth2 -> stdDev_depth1 ~ stdDev_depth2
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    error2 += weightedErrorDepth*weightedErrorDepth;
                                    //cout << "depth err " << weightedErrorDepth << endl;
                                    ++numValidPts;
                                }
                            }
                        }
                    }
                }
            //}
        }
        std::cout << "error2 " << error2 << " numValidPts " << numValidPts << std::endl;
        return sqrt(error2 / numValidPts);
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
    This is done following the work in:
    Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
    in Computer Vision Workshops (ICCV Workshops), 2011. */
    void calcHessGrad_sphere(   const int &pyramidLevel,
                                const Eigen::Matrix4f poseGuess, // The relative pose of the robot between the two frames
                                costFuncType method = PHOTO_CONSISTENCY )
    {
        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        Eigen::MatrixXf jacobiansPhoto(imgSize,6);
        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::MatrixXf jacobiansDepth(imgSize,6);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXi validPixelsPhoto = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXi validPixelsDepth = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        // depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        int numVisiblePixels = 0;

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());
        }

//        if(bUseSalientPixels)
//        {
//#if ENABLE_OPENMP
//#pragma omp parallel for
//#endif
//            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
//            {
//                int r = vSalientPixels[pyramidLevel][i] / nCols;
//                int c = vSalientPixels[pyramidLevel][i] % nCols;
////            cout << "vSalientPixels[pyramidLevel][i] " << vSalientPixels[pyramidLevel][i] << " " << r << " " << c << endl;

//                float phi = (half_nRows-r)*angle_res;
//                float theta = c*angle_res;

//                //Compute the 3D coordinates of the pij of the source frame
//                Eigen::Vector3f point3D;
//                float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
////            std::cout << " d " << depth;
//                if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
//                {
//                    point3D(0) = depth1*sin(phi);
//                    point3D(1) = -depth1*cos(phi)*sin(theta);
//                    point3D(2) = -depth1*cos(phi)*cos(theta);

//                    //Transform the 3D point using the transformation matrix Rt
//                    Eigen::Vector3f rotatedPoint3D = rotation*point3D;
//                    Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
////                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

//                    //Project the 3D point to the S2 sphere
//                    float dist_inv = 1.f / transformedPoint3D.norm();
//                    float phi_trg = asin(transformedPoint3D(0)*dist_inv);
//                    float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
//                    int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
//                    int transformed_c_int = round(theta_trg*angle_res_inv);
////                cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
//                    //Asign the intensity value to the warped image and compute the difference between the transformed
//                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
//                    if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
//                    {
//                        assert(transformed_c_int >= 0 && transformed_c_int < nCols);

//                        //Compute the pixel jacobian
//                        Eigen::Matrix<float,3,6> jacobianT36;
//                        jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
//                        jacobianT36.block(0,3,3,3) = -skew( rotatedPoint3D );

//                        // The Jacobian of the spherical projection
//                        Eigen::Matrix<float,2,3> jacobianProj23;
//                        // Jacobian of theta with respect to x,y,z
//                        float z_inv = 1.f / transformedPoint3D(2);
//                        float z_inv2 = z_inv*z_inv;
//                        float D_atan_theta = 1.f / (1 + transformedPoint3D(1)*transformedPoint3D(1)*z_inv2) *angle_res_inv;
//                        jacobianProj23(0,0) = 0;
//                        jacobianProj23(0,1) = D_atan_theta * z_inv;
//                        jacobianProj23(0,2) = -transformedPoint3D(1) * z_inv2 * D_atan_theta;
//                        // Jacobian of theta with respect to x,y,z
//                        float dist_inv2 = dist_inv*dist_inv;
//                        float x_dist_inv2 = transformedPoint3D(0)*dist_inv2;
//                        float D_asin = 1.f / sqrt(1-transformedPoint3D(0)*x_dist_inv2) *angle_res_inv;
//                        jacobianProj23(1,0) = -D_asin * dist_inv * (1 - transformedPoint3D(0)*x_dist_inv2);
//                        jacobianProj23(1,1) = D_asin * (x_dist_inv2*transformedPoint3D(1)*dist_inv);
//                        jacobianProj23(1,2) = D_asin * (x_dist_inv2*transformedPoint3D(2)*dist_inv);
////                        float y2_z2_inv = 1.f / (transformedPoint3D(1)*transformedPoint3D(1) + transformedPoint3D(2)*transformedPoint3D(2));
////                        float sq_y2_z2 = sqrt(y2_z2_inv);
////                        float D_atan = 1 / (1 + transformedPoint3D(0)*transformedPoint3D(0)*y2_z2_inv);
////                        jacobianProj23(1,0) = - D_atan * sq_y2_z2;
////                        jacobianProj23(1,1) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(1);
////                        jacobianProj23(1,2) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(2);
////                    std::cout << "jacobianProj23 \n " << jacobianProj23 << " \n D_phi_x " << D_phi_x << " " << D_phi_y << " " << D_phi_z << " " << std::endl;

//                        Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;

//                        float pixel1, pixel2, depth2;
//                        float weightedErrorPhoto, weightedErrorDepth;
//                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

//                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the pixel values that will be used to compute the pixel residual
//                            pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
//                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
////                        std::cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << std::endl;
//                            float photoDiff = pixel2 - pixel1;
//                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
////                            if(photoDiff2 > varianceRegularization)
////                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
//                            weightedErrorPhoto = weight_photo * photoDiff;

//                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
//                            Eigen::Matrix<float,1,2> target_imgGradient;
//                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
//                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
////                        std::cout << "weight_photo " << weight_photo << " target_imgGradient " << target_imgGradient << "\njacobianWarpRt\n" << jacobianWarpRt << std::endl;
////                        std::cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;

//                            if(visualizeIterations)
//                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = pixel1;
//                        }
//                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                        {
//                            //Obtain the depth values that will be used to the compute the depth residual
//                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                float depthDiff = depth2 - depth1;
//                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
//                                float stdDev_depth1 = stdDevDepth*depth1;
//                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
//                                weightedErrorDepth = weight_depth * depthDiff;

//                                //Depth jacobian:
//                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
//                                Eigen::Matrix<float,1,2> target_depthGradient;
//                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
//                                Eigen::Matrix<float,1,3> jacobianDepthSrc = transformedPoint3D*dist_inv;
//                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianDepthSrc*jacobianT36);
////                            std::cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << std::endl;

//                                if(visualizeIterations)
//                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = depth1;
//                            }
//                        }

//                        //Assign the pixel residual and jacobian to its corresponding row
//#if ENABLE_OPENMP
//#pragma omp critical
//#endif
//                        {
//                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                            {
//                                // Photometric component
//                                hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
//                            }
//                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                            {
//                                // Depth component (Plane ICL like)
//                                hessian += jacobianDepth.transpose()*jacobianDepth;
//                                gradient += jacobianDepth.transpose()*weightedErrorDepth;
//                            }
//                        }

//                    }
//                }
//            }
//        }
//        else
        {
            // int countSalientPix = 0;
#if ENABLE_OPENMP
#pragma omp parallel for reduction(+:numVisiblePixels)
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                // float phi = (half_nRows-r)*angle_res;
//                // float sin_phi = sin(phi);
//                // float cos_phi = cos(phi);
//                for(int c=0;c<nCols;c++)
//                {
                    // float theta = c*angle_res;
                    // {
                    // int size_img = nRows*nCols;
                 for(int i=0; i < LUT_xyz_sphere.size(); i++)
                 {
                    //Compute the 3D coordinates of the pij of the source frame
                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    // std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                    //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                        // if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
                    {
                        // Eigen::Vector3f point3D = LUT_xyz_sphere[i];
                        // point3D(0) = depth1*sin_phi;
                        // point3D(1) = -depth1*cos_phi*sin(theta);
                        // point3D(2) = -depth1*cos_phi*cos(theta);
                        // point3D(1) = depth1*sin(phi);
                        // point3D(0) = depth1*cos(phi)*sin(theta);
                        // point3D(2) = -depth1*cos(phi)*cos(theta);
                        //Transform the 3D point using the transformation matrix Rt
                        // Eigen::Vector3f rotatedPoint3D = rotation*point3D;
                        // Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
                        //Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;
                        // cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;
                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
                        // float phi_trg = asin(transformedPoint3D(1)*dist_inv);
                        // float theta_trg = atan2(transformedPoint3D(1),-transformedPoint3D(2))+PI;
                        // int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        // int transformed_c_int = round(theta_trg*angle_res_inv);
                        // cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
                            ++numVisiblePixels;

                            // assert(transformed_c_int >= 0 && transformed_c_int < nCols);
                            //Compute the pixel jacobian
                            Eigen::Matrix<float,3,6> jacobianT36;
                            jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
                            jacobianT36.block(0,3,3,3) = -skew( transformedPoint3D );

                            // The Jacobian of the spherical projection
                            Eigen::Matrix<float,2,3> jacobianProj23;
                            // Jacobian of theta with respect to x,y,z
                            float z_inv = 1.f / transformedPoint3D(2);
                            float z_inv2 = z_inv*z_inv;
                            float D_atan_theta = 1.f / (1 + transformedPoint3D(1)*transformedPoint3D(1)*z_inv2) *angle_res_inv;
                            jacobianProj23(0,0) = 0;
                            jacobianProj23(0,1) = D_atan_theta * z_inv;
                            jacobianProj23(0,2) = -transformedPoint3D(1) * z_inv2 * D_atan_theta;
                            // jacobianProj23(0,2) = -D_atan_theta * z_inv;
                            // jacobianProj23(0,1) = transformedPoint3D(1) * z_inv2 * D_atan_theta;
                            // Jacobian of phi with respect to x,y,z
                            float dist_inv2 = dist_inv*dist_inv;
                            float x_dist_inv2 = transformedPoint3D(0)*dist_inv2;
                            float D_asin = 1.f / sqrt(1-transformedPoint3D(0)*x_dist_inv2) *angle_res_inv;
                            jacobianProj23(1,0) = -D_asin * dist_inv * (1 - transformedPoint3D(0)*x_dist_inv2);
                            jacobianProj23(1,1) = D_asin * (x_dist_inv2*transformedPoint3D(1)*dist_inv);
                            jacobianProj23(1,2) = D_asin * (x_dist_inv2*transformedPoint3D(2)*dist_inv);
                            // float y2_z2_inv = 1.f / (transformedPoint3D(1)*transformedPoint3D(1) + transformedPoint3D(2)*transformedPoint3D(2));
                            // float sq_y2_z2 = sqrt(y2_z2_inv);
                            // float D_atan = 1 / (1 + transformedPoint3D(0)*transformedPoint3D(0)*y2_z2_inv) *angle_res_inv;;
                            // Eigen::Matrix<float,2,3> jacobianProj23_;
                            // jacobianProj23_(1,0) = - D_atan * sq_y2_z2;
                            // jacobianProj23_(1,1) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(1);
                            // jacobianProj23_(1,2) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(2);
                            // std::cout << "jacobianProj23 \n " << jacobianProj23 << " \n jacobianProj23_ \n " << jacobianProj23_ << std::endl;

                            Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;
                            float pixel1, pixel2, depth2;
                            float weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                //pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                // std::cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << std::endl;
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                // if(photoDiff2 > varianceRegularization)
                                // weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                                weightedErrorPhoto = weight_photo * photoDiff;
                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
                                // std::cout << "weight_photo " << weight_photo << " target_imgGradient " << target_imgGradient << "\njacobianWarpRt\n" << jacobianWarpRt << std::endl;
                                // std::cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;

                                jacobiansPhoto.block(i,0,1,6) = jacobianPhoto;
                                residualsPhoto(i) = weightedErrorPhoto;
                                validPixelsPhoto(i) = 1;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if(visualizeIterations)
                                        warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = dist;

                                    Eigen::Matrix<float,1,2> target_depthGradient;
                                    target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                        continue;

                                    float depthDiff = depth2 - dist;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth2; // We assume that: depth1 ~ depth2 -> stdDev_depth1 ~ stdDev_depth2
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;
                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,3> jacobianDepthSrc = transformedPoint3D*dist_inv;
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianDepthSrc*jacobianT36);
                                    // std::cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << std::endl;

                                    jacobiansDepth.block(i,0,1,6) = jacobianDepth;
                                    residualsDepth(i) = weightedErrorDepth;
                                    validPixelsDepth(i) = 1;
                                }
                            }
                            //Assign the pixel residual and jacobian to its corresponding row
//#if ENABLE_OPENMP
//#pragma omp critical
//#endif
//                            {
//                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//                                {
//                                    // Photometric component
//                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
//                                }
//                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
//                                    {
//                                        // Depth component (Plane ICL like)
//                                        hessian += jacobianDepth.transpose()*jacobianDepth;
//                                        gradient += jacobianDepth.transpose()*weightedErrorDepth;
//                                    }
//                            }
//                        }
                    }
                }
            //}
        }

        // Compute hessian and gradient
        float h11=0,h12=0,h13=0,h14=0,h15=0,h16=0,h22=0,h23=0,h24=0,h25=0,h26=0,h33=0,h34=0,h35=0,h36=0,h44=0,h45=0,h46=0,h55=0,h56=0,h66=0;
        float g1=0,g2=0,g3=0,g4=0,g5=0,g6=0;

        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
            for(int i=0; i < imgSize; i++)
                if(validPixelsPhoto(i))
                {
                    h11 += jacobiansPhoto(i,0)*jacobiansPhoto(i,0);
                    h12 += jacobiansPhoto(i,0)*jacobiansPhoto(i,1);
                    h13 += jacobiansPhoto(i,0)*jacobiansPhoto(i,2);
                    h14 += jacobiansPhoto(i,0)*jacobiansPhoto(i,3);
                    h15 += jacobiansPhoto(i,0)*jacobiansPhoto(i,4);
                    h16 += jacobiansPhoto(i,0)*jacobiansPhoto(i,5);
                    h22 += jacobiansPhoto(i,1)*jacobiansPhoto(i,1);
                    h23 += jacobiansPhoto(i,1)*jacobiansPhoto(i,2);
                    h24 += jacobiansPhoto(i,1)*jacobiansPhoto(i,3);
                    h25 += jacobiansPhoto(i,1)*jacobiansPhoto(i,4);
                    h26 += jacobiansPhoto(i,1)*jacobiansPhoto(i,5);
                    h33 += jacobiansPhoto(i,2)*jacobiansPhoto(i,2);
                    h34 += jacobiansPhoto(i,2)*jacobiansPhoto(i,3);
                    h35 += jacobiansPhoto(i,2)*jacobiansPhoto(i,4);
                    h36 += jacobiansPhoto(i,2)*jacobiansPhoto(i,5);
                    h44 += jacobiansPhoto(i,3)*jacobiansPhoto(i,3);
                    h45 += jacobiansPhoto(i,3)*jacobiansPhoto(i,4);
                    h46 += jacobiansPhoto(i,3)*jacobiansPhoto(i,5);
                    h55 += jacobiansPhoto(i,4)*jacobiansPhoto(i,4);
                    h56 += jacobiansPhoto(i,4)*jacobiansPhoto(i,5);
                    h66 += jacobiansPhoto(i,5)*jacobiansPhoto(i,5);

                    g1 += jacobiansPhoto(i,0)*residualsPhoto(i);
                    g2 += jacobiansPhoto(i,1)*residualsPhoto(i);
                    g3 += jacobiansPhoto(i,2)*residualsPhoto(i);
                    g4 += jacobiansPhoto(i,3)*residualsPhoto(i);
                    g5 += jacobiansPhoto(i,4)*residualsPhoto(i);
                    g6 += jacobiansPhoto(i,5)*residualsPhoto(i);
                }
        }
        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
            for(int i=0; i < imgSize; i++)
                if(validPixelsDepth(i))
                {
                    h11 += jacobiansDepth(i,0)*jacobiansDepth(i,0);
                    h12 += jacobiansDepth(i,0)*jacobiansDepth(i,1);
                    h13 += jacobiansDepth(i,0)*jacobiansDepth(i,2);
                    h14 += jacobiansDepth(i,0)*jacobiansDepth(i,3);
                    h15 += jacobiansDepth(i,0)*jacobiansDepth(i,4);
                    h16 += jacobiansDepth(i,0)*jacobiansDepth(i,5);
                    h22 += jacobiansDepth(i,1)*jacobiansDepth(i,1);
                    h23 += jacobiansDepth(i,1)*jacobiansDepth(i,2);
                    h24 += jacobiansDepth(i,1)*jacobiansDepth(i,3);
                    h25 += jacobiansDepth(i,1)*jacobiansDepth(i,4);
                    h26 += jacobiansDepth(i,1)*jacobiansDepth(i,5);
                    h33 += jacobiansDepth(i,2)*jacobiansDepth(i,2);
                    h34 += jacobiansDepth(i,2)*jacobiansDepth(i,3);
                    h35 += jacobiansDepth(i,2)*jacobiansDepth(i,4);
                    h36 += jacobiansDepth(i,2)*jacobiansDepth(i,5);
                    h44 += jacobiansDepth(i,3)*jacobiansDepth(i,3);
                    h45 += jacobiansDepth(i,3)*jacobiansDepth(i,4);
                    h46 += jacobiansDepth(i,3)*jacobiansDepth(i,5);
                    h55 += jacobiansDepth(i,4)*jacobiansDepth(i,4);
                    h56 += jacobiansDepth(i,4)*jacobiansDepth(i,5);
                    h66 += jacobiansDepth(i,5)*jacobiansDepth(i,5);

                    g1 += jacobiansDepth(i,0)*residualsDepth(i);
                    g2 += jacobiansDepth(i,1)*residualsDepth(i);
                    g3 += jacobiansDepth(i,2)*residualsDepth(i);
                    g4 += jacobiansDepth(i,3)*residualsDepth(i);
                    g5 += jacobiansDepth(i,4)*residualsDepth(i);
                    g6 += jacobiansDepth(i,5)*residualsDepth(i);
                }
        }
        // Assign the values for the hessian and gradient
        hessian(0,0) = h11;
        hessian(0,1) = hessian(1,0) = h12;
        hessian(0,2) = hessian(2,0) = h13;
        hessian(0,3) = hessian(3,0) = h14;
        hessian(0,4) = hessian(4,0) = h15;
        hessian(0,5) = hessian(5,0) = h16;
        hessian(1,1) = h22;
        hessian(1,2) = hessian(2,1) = h23;
        hessian(1,3) = hessian(3,1) = h24;
        hessian(1,4) = hessian(4,1) = h25;
        hessian(1,5) = hessian(5,1) = h26;
        hessian(2,2) = h33;
        hessian(2,3) = hessian(3,2) = h34;
        hessian(2,4) = hessian(4,2) = h35;
        hessian(2,5) = hessian(5,2) = h36;
        hessian(3,3) = h44;
        hessian(3,4) = hessian(4,3) = h45;
        hessian(3,5) = hessian(5,3) = h46;
        hessian(4,4) = h55;
        hessian(4,5) = hessian(5,4) = h56;
        hessian(5,5) = h66;

        gradient(0) = g1;
        gradient(1) = g2;
        gradient(2) = g3;
        gradient(3) = g4;
        gradient(4) = g5;
        gradient(5) = g6;
    }
    SSO = (float)numVisiblePixels / imgSize;
//        std::cout << "numVisiblePixels " << numVisiblePixels << " imgSize " << imgSize << " sso " << SSO << std::endl;
}

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        Occlusions are taken into account by a Z-buffer. */
    double errorPhotoICP_sphereOcc1(const int &pyramidLevel,
                                    const Eigen::Matrix4f &poseGuess, // The relative pose of the robot between the two frames
                                    costFuncType method = PHOTO_CONSISTENCY )
    {
//        double error2 = 0.0;
        int nValidPhotoPts = 0;
        int nValidDepthPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const int imgSize = nRows*nCols;

        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        double stdDevPhoto_inv = 1./stdDevPhoto;
        double stdDevDepth_inv = 1./stdDevDepth;

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:nValidDepthPts) // nValidPhotoPts,  //for reduction (+:error2)
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                for(int c=0;c<nCols;c++)
                for(int i=0; i < LUT_xyz_sphere.size(); i++)
                {
                    //Compute the 3D coordinates of the pij of the source frame
                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                    {
                        Eigen::Vector3f point3D = LUT_xyz_sphere[i]; // The LUT allows to not recalculate the source point cloud for each iteration

                        //Transform the 3D point using the transformation matrix Rt
                        Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
    //                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
    //                cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
//                        cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << " " << nRows << "x" << nCols << endl;
                            assert(transformed_c_int >= 0 && transformed_c_int < nCols);

                            // Discard occluded points
                            int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                            if(invDepthBuffer(ii) > 0 && dist_inv < invDepthBuffer(ii)) // the current pixel is occluded
                                continue;
                            invDepthBuffer(ii) = dist_inv;


                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                // Filter the pixels according to the image gradients
                                if(fabs(grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity &&
                                   fabs(grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                //float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                residualsPhoto(ii) = weightedErrorPhoto*weightedErrorPhoto;
                                ++nValidPhotoPts;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Filter the pixels according to the image gradients
                                    if(fabs(depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth &&
                                       fabs(depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth)
                                        continue;

                                    //Obtain the depth values that will be used to the compute the depth residual
                                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                    float depthDiff = depth2 - dist;
                                    float stdDev_depth1 = stdDevDepth*depth2;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    residualsDepth(ii) = weightedErrorDepth*weightedErrorDepth;
                                    ++nValidDepthPts;
                                }
                            }
                        }
                    }
                }
            //}
        }

        double PhotoResidual = 0.0;
        double DepthResidual = 0.0;
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual) // nValidPhotoPts, nValidDepthPts
#endif
        for(int i=0; i < imgSize; i++)
        {
            PhotoResidual += residualsPhoto(i);
            DepthResidual += residualsDepth(i);
//                if(residualsPhoto(i) > 0) ++nValidPhotoPts;
//                if(residualsDepth(i) > 0) ++nValidDepthPts;
        }

        avPhotoResidual = sqrt(PhotoResidual / nValidPhotoPts);
        //avPhotoResidual = sqrt(PhotoResidual / nValidDepthPts);
        avDepthResidual = sqrt(DepthResidual / nValidDepthPts);

        std::cout << "avDepthResidual " << avDepthResidual << " DepthResidual " << DepthResidual << std::endl;
        // std::cout << "nValidPhotoPts " << nValidPhotoPts << " nValidDepthPts " << nValidDepthPts << std::endl;

        return avPhotoResidual + avDepthResidual;
        //return error2;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        This function takes into account the occlusions by storing a Z-buffer */
    void calcHessGrad_sphereOcc1( const int &pyramidLevel,
                                    const Eigen::Matrix4f poseGuess, // The relative pose of the robot between the two frames
                                    costFuncType method = PHOTO_CONSISTENCY )
    {
        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        Eigen::MatrixXf jacobiansPhoto(imgSize,6);
        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::MatrixXf jacobiansDepth(imgSize,6);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXi validPixelsPhoto = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXi validPixelsDepth = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        int numVisiblePixels = 0;

        //std::cout << " calcHessianAndGradient_sphere visualizeIterations " << visualizeIterations << std::endl;
        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());
        }

        {
//            cout << "compute hessian/gradient " << endl;
//        int countSalientPix = 0;
#if ENABLE_OPENMP
#pragma omp parallel for reduction(+:numVisiblePixels)
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                // float phi = (half_nRows-r)*angle_res;
//                // float sin_phi = sin(phi);
//                // float cos_phi = cos(phi);
//                for(int c=0;c<nCols;c++)
//                {
                    // float theta = c*angle_res;
                    // {
                    // int size_img = nRows*nCols;
                 for(int i=0; i < LUT_xyz_sphere.size(); i++)
                 {
                    //Compute the 3D coordinates of the pij of the source frame
                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    // std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                    //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                        // if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
                    {
                        // Eigen::Vector3f point3D = LUT_xyz_sphere[i];
                        // point3D(0) = depth1*sin_phi;
                        // point3D(1) = -depth1*cos_phi*sin(theta);
                        // point3D(2) = -depth1*cos_phi*cos(theta);
                        // point3D(1) = depth1*sin(phi);
                        // point3D(0) = depth1*cos(phi)*sin(theta);
                        // point3D(2) = -depth1*cos(phi)*cos(theta);
                        //Transform the 3D point using the transformation matrix Rt
                        // Eigen::Vector3f rotatedPoint3D = rotation*point3D;
                        // Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
                        //Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;
    //                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
//                        float phi_trg = asin(transformedPoint3D(1)*dist_inv);
//                        float theta_trg = atan2(transformedPoint3D(1),-transformedPoint3D(2))+PI;
//                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
//                        int transformed_c_int = round(theta_trg*angle_res_inv);
//                    cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
//                            assert(transformed_c_int >= 0 && transformed_c_int < nCols);
                            // Discard occluded points
                            if(invDepthBuffer(i) > 0 && dist_inv < invDepthBuffer(i))
                                continue;
                            invDepthBuffer(i) = dist_inv;

                            ++numVisiblePixels;

                            //Compute the pixel jacobian
                            Eigen::Matrix<float,3,6> jacobianT36;
                            jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
                            jacobianT36.block(0,3,3,3) = -skew( transformedPoint3D );

                            // The Jacobian of the spherical projection
                            Eigen::Matrix<float,2,3> jacobianProj23;
                            // Jacobian of theta with respect to x,y,z
                            float z_inv = 1.f / transformedPoint3D(2);
                            float z_inv2 = z_inv*z_inv;
                            float D_atan_theta = 1.f / (1 + transformedPoint3D(1)*transformedPoint3D(1)*z_inv2) *angle_res_inv;
                            jacobianProj23(0,0) = 0;
                            jacobianProj23(0,1) = D_atan_theta * z_inv;
                            jacobianProj23(0,2) = -transformedPoint3D(1) * z_inv2 * D_atan_theta;
    //                        jacobianProj23(0,2) = -D_atan_theta * z_inv;
    //                        jacobianProj23(0,1) = transformedPoint3D(1) * z_inv2 * D_atan_theta;
                            // Jacobian of phi with respect to x,y,z
                            float dist_inv2 = dist_inv*dist_inv;
                            float x_dist_inv2 = transformedPoint3D(0)*dist_inv2;
                            float D_asin = 1.f / sqrt(1-transformedPoint3D(0)*x_dist_inv2) *angle_res_inv;
                            jacobianProj23(1,0) = -D_asin * dist_inv * (1 - transformedPoint3D(0)*x_dist_inv2);
                            jacobianProj23(1,1) = D_asin * (x_dist_inv2*transformedPoint3D(1)*dist_inv);
                            jacobianProj23(1,2) = D_asin * (x_dist_inv2*transformedPoint3D(2)*dist_inv);
//                            float y2_z2_inv = 1.f / (transformedPoint3D(1)*transformedPoint3D(1) + transformedPoint3D(2)*transformedPoint3D(2));
//                            float sq_y2_z2 = sqrt(y2_z2_inv);
//                            float D_atan = 1 / (1 + transformedPoint3D(0)*transformedPoint3D(0)*y2_z2_inv) *angle_res_inv;;
//                            Eigen::Matrix<float,2,3> jacobianProj23_;
//                            jacobianProj23_(1,0) = - D_atan * sq_y2_z2;
//                            jacobianProj23_(1,1) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(1);
//                            jacobianProj23_(1,2) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(2);
//                        std::cout << "jacobianProj23 \n " << jacobianProj23 << " \n jacobianProj23_ \n " << jacobianProj23_ << std::endl;

                            Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;

                            float pixel1, pixel2, depth2;
                            float weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                //pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
    //                        std::cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << std::endl;
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
    //                            if(photoDiff2 > varianceRegularization)
    //                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                                weightedErrorPhoto = weight_photo * photoDiff;

                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
                            //std::cout << "weight_photo " << weight_photo << " target_imgGradient " << target_imgGradient << "\njacobianWarpRt\n" << jacobianWarpRt << std::endl;
                            //std::cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if(visualizeIterations)
                                        warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = dist;

                                    Eigen::Matrix<float,1,2> target_depthGradient;
                                    target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                        continue;

                                    float depthDiff = depth2 - dist;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth2; // We assume that: depth1 ~ depth2 -> stdDev_depth1 ~ stdDev_depth2
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;

                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,3> jacobianDepthSrc = transformedPoint3D*dist_inv;
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianDepthSrc*jacobianT36);
                                //std::cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << std::endl;
                                }
                            }

                            //Assign the pixel residual and jacobian to its corresponding row. The critical section avoids very weird situations, is it necessary?
    #if ENABLE_OPENMP
    #pragma omp critical
    #endif
                            {
                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                {
                                    // Photometric component
//                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
                                    // Take into account the possible occlusions
                                    jacobiansPhoto.block(i,0,1,6) = jacobianPhoto;
                                    residualsPhoto(i) = weightedErrorPhoto;
                                    validPixelsPhoto(i) = 1;
                                }
                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Depth component (Plane ICL like)
//                                    hessian += jacobianDepth.transpose()*jacobianDepth;
//                                    gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                    // Take into account the possible occlusions
                                    jacobiansDepth.block(i,0,1,6) = jacobianDepth;
                                    residualsDepth(i) = weightedErrorDepth;
                                    validPixelsDepth(i) = 1;
                                }
                            }
                        }
                    }
                }
            //}
            // Compute hessian and gradient
            float h11=0,h12=0,h13=0,h14=0,h15=0,h16=0,h22=0,h23=0,h24=0,h25=0,h26=0,h33=0,h34=0,h35=0,h36=0,h44=0,h45=0,h46=0,h55=0,h56=0,h66=0;
            float g1=0,g2=0,g3=0,g4=0,g5=0,g6=0;

            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
            {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
                for(int i=0; i < imgSize; i++)
                    if(validPixelsPhoto(i))
                    {
                        h11 += jacobiansPhoto(i,0)*jacobiansPhoto(i,0);
                        h12 += jacobiansPhoto(i,0)*jacobiansPhoto(i,1);
                        h13 += jacobiansPhoto(i,0)*jacobiansPhoto(i,2);
                        h14 += jacobiansPhoto(i,0)*jacobiansPhoto(i,3);
                        h15 += jacobiansPhoto(i,0)*jacobiansPhoto(i,4);
                        h16 += jacobiansPhoto(i,0)*jacobiansPhoto(i,5);
                        h22 += jacobiansPhoto(i,1)*jacobiansPhoto(i,1);
                        h23 += jacobiansPhoto(i,1)*jacobiansPhoto(i,2);
                        h24 += jacobiansPhoto(i,1)*jacobiansPhoto(i,3);
                        h25 += jacobiansPhoto(i,1)*jacobiansPhoto(i,4);
                        h26 += jacobiansPhoto(i,1)*jacobiansPhoto(i,5);
                        h33 += jacobiansPhoto(i,2)*jacobiansPhoto(i,2);
                        h34 += jacobiansPhoto(i,2)*jacobiansPhoto(i,3);
                        h35 += jacobiansPhoto(i,2)*jacobiansPhoto(i,4);
                        h36 += jacobiansPhoto(i,2)*jacobiansPhoto(i,5);
                        h44 += jacobiansPhoto(i,3)*jacobiansPhoto(i,3);
                        h45 += jacobiansPhoto(i,3)*jacobiansPhoto(i,4);
                        h46 += jacobiansPhoto(i,3)*jacobiansPhoto(i,5);
                        h55 += jacobiansPhoto(i,4)*jacobiansPhoto(i,4);
                        h56 += jacobiansPhoto(i,4)*jacobiansPhoto(i,5);
                        h66 += jacobiansPhoto(i,5)*jacobiansPhoto(i,5);

                        g1 += jacobiansPhoto(i,0)*residualsPhoto(i);
                        g2 += jacobiansPhoto(i,1)*residualsPhoto(i);
                        g3 += jacobiansPhoto(i,2)*residualsPhoto(i);
                        g4 += jacobiansPhoto(i,3)*residualsPhoto(i);
                        g5 += jacobiansPhoto(i,4)*residualsPhoto(i);
                        g6 += jacobiansPhoto(i,5)*residualsPhoto(i);
                    }
            }
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
            {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
                for(int i=0; i < imgSize; i++)
                    if(validPixelsDepth(i))
                    {
                        h11 += jacobiansDepth(i,0)*jacobiansDepth(i,0);
                        h12 += jacobiansDepth(i,0)*jacobiansDepth(i,1);
                        h13 += jacobiansDepth(i,0)*jacobiansDepth(i,2);
                        h14 += jacobiansDepth(i,0)*jacobiansDepth(i,3);
                        h15 += jacobiansDepth(i,0)*jacobiansDepth(i,4);
                        h16 += jacobiansDepth(i,0)*jacobiansDepth(i,5);
                        h22 += jacobiansDepth(i,1)*jacobiansDepth(i,1);
                        h23 += jacobiansDepth(i,1)*jacobiansDepth(i,2);
                        h24 += jacobiansDepth(i,1)*jacobiansDepth(i,3);
                        h25 += jacobiansDepth(i,1)*jacobiansDepth(i,4);
                        h26 += jacobiansDepth(i,1)*jacobiansDepth(i,5);
                        h33 += jacobiansDepth(i,2)*jacobiansDepth(i,2);
                        h34 += jacobiansDepth(i,2)*jacobiansDepth(i,3);
                        h35 += jacobiansDepth(i,2)*jacobiansDepth(i,4);
                        h36 += jacobiansDepth(i,2)*jacobiansDepth(i,5);
                        h44 += jacobiansDepth(i,3)*jacobiansDepth(i,3);
                        h45 += jacobiansDepth(i,3)*jacobiansDepth(i,4);
                        h46 += jacobiansDepth(i,3)*jacobiansDepth(i,5);
                        h55 += jacobiansDepth(i,4)*jacobiansDepth(i,4);
                        h56 += jacobiansDepth(i,4)*jacobiansDepth(i,5);
                        h66 += jacobiansDepth(i,5)*jacobiansDepth(i,5);

                        g1 += jacobiansDepth(i,0)*residualsDepth(i);
                        g2 += jacobiansDepth(i,1)*residualsDepth(i);
                        g3 += jacobiansDepth(i,2)*residualsDepth(i);
                        g4 += jacobiansDepth(i,3)*residualsDepth(i);
                        g5 += jacobiansDepth(i,4)*residualsDepth(i);
                        g6 += jacobiansDepth(i,5)*residualsDepth(i);
                    }
            }
            // Assign the values for the hessian and gradient
            hessian(0,0) = h11;
            hessian(0,1) = hessian(1,0) = h12;
            hessian(0,2) = hessian(2,0) = h13;
            hessian(0,3) = hessian(3,0) = h14;
            hessian(0,4) = hessian(4,0) = h15;
            hessian(0,5) = hessian(5,0) = h16;
            hessian(1,1) = h22;
            hessian(1,2) = hessian(2,1) = h23;
            hessian(1,3) = hessian(3,1) = h24;
            hessian(1,4) = hessian(4,1) = h25;
            hessian(1,5) = hessian(5,1) = h26;
            hessian(2,2) = h33;
            hessian(2,3) = hessian(3,2) = h34;
            hessian(2,4) = hessian(4,2) = h35;
            hessian(2,5) = hessian(5,2) = h36;
            hessian(3,3) = h44;
            hessian(3,4) = hessian(4,3) = h45;
            hessian(3,5) = hessian(5,3) = h46;
            hessian(4,4) = h55;
            hessian(4,5) = hessian(5,4) = h56;
            hessian(5,5) = h66;

            gradient(0) = g1;
            gradient(1) = g2;
            gradient(2) = g3;
            gradient(3) = g4;
            gradient(4) = g5;
            gradient(5) = g6;
        }
        SSO = (float)numVisiblePixels / imgSize;
//        std::cout << "numVisiblePixels " << numVisiblePixels << " imgSize " << imgSize << " sso " << SSO << std::endl;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        Occlusions are taken into account by a Z-buffer. */
    double errorPhotoICP_sphereOcc2(const int &pyramidLevel,
                                    const Eigen::Matrix4f &poseGuess, // The relative pose of the robot between the two frames
                                    costFuncType method = PHOTO_CONSISTENCY )
    {
//        double error2 = 0.0;
        double PhotoResidual = 0.0;
        double DepthResidual = 0.0;
        int nValidPhotoPts = 0;
        int nValidDepthPts = 0;

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const int imgSize = nRows*nCols;

        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        double stdDevPhoto_inv = 1./stdDevPhoto;
        double stdDevDepth_inv = 1./stdDevDepth;

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:nValidDepthPts) // nValidPhotoPts,  //for reduction (+:error2)
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                for(int c=0;c<nCols;c++)
            for(int i=0; i < LUT_xyz_sphere.size(); i++)
            {
                //Compute the 3D coordinates of the pij of the source frame
                //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                //int i = r*nCols + c;
                if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                {
                    Eigen::Vector3f point3D = LUT_xyz_sphere[i]; // The LUT allows to not recalculate the source point cloud for each iteration

                        //Transform the 3D point using the transformation matrix Rt
                        Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
    //                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
    //                cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
//                        cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << " " << nRows << "x" << nCols << endl;
                            assert(transformed_c_int >= 0 && transformed_c_int < nCols);

                            // Discard outliers (occluded and moving points)
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            float depthDiff = depth2 - dist;
                            if(fabs(depthDiff) > thresDepthOutliers)
                                continue;

                            // Discard occluded points
                            int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                            if(invDepthBuffer(ii) > 0 && dist_inv < invDepthBuffer(ii)) // the current pixel is occluded
                                continue;
                            invDepthBuffer(ii) = dist_inv;

                            ++nValidDepthPts;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                // Filter the pixels according to the image gradients
                                if(fabs(grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity &&
                                   fabs(grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
                                float weightedErrorPhoto = weight_photo * photoDiff;
                                residualsPhoto(i) = weightedErrorPhoto*weightedErrorPhoto;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Filter the pixels according to the image gradients
                                    if(fabs(depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth &&
                                       fabs(depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int)) < thresSaliencyDepth )
                                        continue;

                                    //Obtain the depth values that will be used to the compute the depth residual
                                    //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                    float stdDev_depth1 = stdDevDepth*depth2;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    residualsDepth(i) = weightedErrorDepth*weightedErrorDepth;
//                                    ++nValidDepthPts;
                                }
                            }
                        }
                    }
                }
            //}
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:PhotoResidual,DepthResidual) // nValidPhotoPts, nValidDepthPts
#endif
            for(int i=0; i < imgSize; i++)
            {
//                std::cout << "residualsPhoto(i) " << residualsPhoto(i) << std::endl;
                PhotoResidual += residualsPhoto(i);
                DepthResidual += residualsDepth(i);
//                if(residualsPhoto(i) > 0) ++nValidPhotoPts;
//                if(residualsDepth(i) > 0) ++nValidDepthPts;
            }
        }

        //avPhotoResidual = sqrt(PhotoResidual / nValidPhotoPts);
        avPhotoResidual = sqrt(PhotoResidual / nValidDepthPts);
        avDepthResidual = sqrt(DepthResidual / nValidDepthPts);
        //std::cout << "avPhotoResidual " << avPhotoResidual << " PhotoResidual " << PhotoResidual << " nValidDepthPts " << nValidDepthPts << std::endl;
        return avPhotoResidual + avDepthResidual;
        //return error2;
    }

    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        This function takes into account the occlusions and the moving pixels by applying a filter on the maximum depth error */
    void calcHessGrad_sphereOcc2( const int &pyramidLevel,
                                    const Eigen::Matrix4f poseGuess, // The relative pose of the robot between the two frames
                                    costFuncType method = PHOTO_CONSISTENCY )
    {
        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;
        const int imgSize = nRows*nCols;

        const float angle_res = 2*PI/nCols;
        const float angle_res_inv = 1/angle_res;
        const float phi_FoV = angle_res*nRows; // The vertical FOV in radians
        const float half_nRows = 0.5*nRows-0.5;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        Eigen::MatrixXf jacobiansPhoto(imgSize,6);
        Eigen::VectorXf residualsPhoto = Eigen::VectorXf::Zero(imgSize);
        Eigen::MatrixXf jacobiansDepth(imgSize,6);
        Eigen::VectorXf residualsDepth = Eigen::VectorXf::Zero(imgSize);
        Eigen::VectorXi validPixelsPhoto = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXi validPixelsDepth = Eigen::VectorXi::Zero(imgSize);
        Eigen::VectorXf invDepthBuffer = Eigen::VectorXf::Zero(imgSize);

        const Eigen::Matrix3f rotation = poseGuess.block(0,0,3,3);
        const Eigen::Vector3f translation = poseGuess.block(0,3,3,1);

        float weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
//        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        float stdDevPhoto_inv = 1./stdDevPhoto;
        float stdDevDepth_inv = 1./stdDevDepth;

        int numVisiblePixels = 0;

        //std::cout << " calcHessianAndGradient_sphere visualizeIterations " << visualizeIterations << std::endl;
        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());

            // Initialize the mask to segment the dynamic objects and the occlusions
            mask_dynamic_occlusion = cv::Mat::zeros(nRows, nCols, CV_8U);
        }

        Eigen::VectorXf correspTrgSrc = Eigen::VectorXf::Zero(imgSize);
        std::vector<float> depthDiff_(imgSize,-1);

        {
//            cout << "compute hessian/gradient " << endl;
//        int countSalientPix = 0;
#if ENABLE_OPENMP
#pragma omp parallel for
#endif
//            for(int r=0;r<nRows;r++)
//            {
//                // float phi = (half_nRows-r)*angle_res;
//                // float sin_phi = sin(phi);
//                // float cos_phi = cos(phi);
//                for(int c=0;c<nCols;c++)
//                {
                    // float theta = c*angle_res;
                    // {
                    // int size_img = nRows*nCols;
                for(int i=0; i < LUT_xyz_sphere.size(); i++)
                {
                   //Compute the 3D coordinates of the pij of the source frame
                   //float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
                   // std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                   //int i = r*nCols + c;
                    if(LUT_xyz_sphere[i](0) != INVALID_POINT) //Compute the jacobian only for the valid points
                        // if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
                    {
                        // Eigen::Vector3f point3D = LUT_xyz_sphere[i];
                        // point3D(0) = depth1*sin_phi;
                        // point3D(1) = -depth1*cos_phi*sin(theta);
                        // point3D(2) = -depth1*cos_phi*cos(theta);
                        // point3D(1) = depth1*sin(phi);
                        // point3D(0) = depth1*cos(phi)*sin(theta);
                        // point3D(2) = -depth1*cos(phi)*cos(theta);
                        //Transform the 3D point using the transformation matrix Rt
                        // Eigen::Vector3f rotatedPoint3D = rotation*point3D;
                        // Eigen::Vector3f transformedPoint3D = rotatedPoint3D + translation;
                        //Eigen::Vector3f transformedPoint3D = rotation*point3D + translation;
                        Eigen::Vector3f transformedPoint3D = rotation*LUT_xyz_sphere[i] + translation;
    //                cout << "3D pts " << point3D.transpose() << " trnasformed " << transformedPoint3D.transpose() << endl;

                        //Project the 3D point to the S2 sphere
                        float dist = transformedPoint3D.norm();
                        float dist_inv = 1.f / dist;
                        float phi_trg = asin(transformedPoint3D(0)*dist_inv);
                        float theta_trg = atan2(transformedPoint3D(1),transformedPoint3D(2))+PI;
                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
                        int transformed_c_int = round(theta_trg*angle_res_inv);
//                        float phi_trg = asin(transformedPoint3D(1)*dist_inv);
//                        float theta_trg = atan2(transformedPoint3D(1),-transformedPoint3D(2))+PI;
//                        int transformed_r_int = round(half_nRows-phi_trg*angle_res_inv);
//                        int transformed_c_int = round(theta_trg*angle_res_inv);
//                    cout << "Pixel transform " << r << " " << c << " " << transformed_r_int << " " << transformed_c_int << endl;
                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) && transformed_c_int < nCols )
                        {
//                            assert(transformed_c_int >= 0 && transformed_c_int < nCols);

                            // Discard outliers (occluded and moving points)
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            float depthDiff = depth2 - dist;
                            if(fabs(depthDiff) > thresDepthOutliers)
                            {
                                if(visualizeIterations)
                                    if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                    {
                                        if(depthDiff > 0)
                                            mask_dynamic_occlusion.at<uchar>(i) = 255;
                                        else
                                            mask_dynamic_occlusion.at<uchar>(i) = 155;
                                    }
                                //assert(false);
                                continue;
                            }

                            // Discard occluded points
                            int ii = transformed_r_int*nCols + transformed_c_int; // The index of the pixel in the target image
                            if(invDepthBuffer(ii) == 0)
                                ++numVisiblePixels;
//                            else
//                            {
//                                if(dist_inv < invDepthBuffer(ii)) // the current pixel is occluded
//                                {
//                                    mask_dynamic_occlusion.at<uchar>(i) = 55;
//                                    continue;
//                                }
//                                else // The previous pixel used was occluded
//                                    mask_dynamic_occlusion.at<uchar>(correspTrgSrc[ii]) = 55;
//                            }

                            invDepthBuffer(ii) = dist_inv;
                            correspTrgSrc(ii) = i;

                            //Compute the pixel jacobian
                            Eigen::Matrix<float,3,6> jacobianT36;
                            jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
                            jacobianT36.block(0,3,3,3) = -skew( transformedPoint3D );

                            // The Jacobian of the spherical projection
                            Eigen::Matrix<float,2,3> jacobianProj23;
                            // Jacobian of theta with respect to x,y,z
                            float z_inv = 1.f / transformedPoint3D(2);
                            float z_inv2 = z_inv*z_inv;
                            float D_atan_theta = 1.f / (1 + transformedPoint3D(1)*transformedPoint3D(1)*z_inv2) *angle_res_inv;
                            jacobianProj23(0,0) = 0;
                            jacobianProj23(0,1) = D_atan_theta * z_inv;
                            jacobianProj23(0,2) = -transformedPoint3D(1) * z_inv2 * D_atan_theta;
    //                        jacobianProj23(0,2) = -D_atan_theta * z_inv;
    //                        jacobianProj23(0,1) = transformedPoint3D(1) * z_inv2 * D_atan_theta;
                            // Jacobian of phi with respect to x,y,z
                            float dist_inv2 = dist_inv*dist_inv;
                            float x_dist_inv2 = transformedPoint3D(0)*dist_inv2;
                            float D_asin = 1.f / sqrt(1-transformedPoint3D(0)*x_dist_inv2) *angle_res_inv;
                            jacobianProj23(1,0) = -D_asin * dist_inv * (1 - transformedPoint3D(0)*x_dist_inv2);
                            jacobianProj23(1,1) = D_asin * (x_dist_inv2*transformedPoint3D(1)*dist_inv);
                            jacobianProj23(1,2) = D_asin * (x_dist_inv2*transformedPoint3D(2)*dist_inv);
//                            float y2_z2_inv = 1.f / (transformedPoint3D(1)*transformedPoint3D(1) + transformedPoint3D(2)*transformedPoint3D(2));
//                            float sq_y2_z2 = sqrt(y2_z2_inv);
//                            float D_atan = 1 / (1 + transformedPoint3D(0)*transformedPoint3D(0)*y2_z2_inv) *angle_res_inv;;
//                            Eigen::Matrix<float,2,3> jacobianProj23_;
//                            jacobianProj23_(1,0) = - D_atan * sq_y2_z2;
//                            jacobianProj23_(1,1) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(1);
//                            jacobianProj23_(1,2) = D_atan * sq_y2_z2*y2_z2_inv*transformedPoint3D(0)*transformedPoint3D(2);
//                        std::cout << "jacobianProj23 \n " << jacobianProj23 << " \n jacobianProj23_ \n " << jacobianProj23_ << std::endl;

                            Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;

                            float pixel1, pixel2;
                            float weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = graySrcPyr[pyramidLevel].at<float>(i);

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(i); // Intensity value of the pixel(r,c) of source frame
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
    //                        std::cout << "pixel1 " << pixel1 << " pixel2 " << pixel2 << std::endl;
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
    //                            if(photoDiff2 > varianceRegularization)
    //                                weight_photo = sqrt(2*stdDevReg*abs(photoDiff2)-varianceRegularization) / photoDiff2_norm;
                                weightedErrorPhoto = weight_photo * photoDiff;

                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
    //                        std::cout << "weight_photo " << weight_photo << " target_imgGradient " << target_imgGradient << "\njacobianWarpRt\n" << jacobianWarpRt << std::endl;
    //                        std::cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    if(visualizeIterations)
                                        warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = dist;

                                    Eigen::Matrix<float,1,2> target_depthGradient;
                                    target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                        continue;

                                    float depthDiff = depth2 - dist;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth2; // We assume that: depth1 ~ depth2 -> stdDev_depth1 ~ stdDev_depth2
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;

                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,3> jacobianDepthSrc = transformedPoint3D*dist_inv;
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianDepthSrc*jacobianT36);
    //                            std::cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << std::endl;
                                }
                            }

                            //Assign the pixel residual and jacobian to its corresponding row. The critical section avoids very weird situations, is it necessary?
    #if ENABLE_OPENMP
    #pragma omp critical
    #endif
                            {
                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                {
                                    // Photometric component
//                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
//                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
                                    // Take into account the possible occlusions
                                    jacobiansPhoto.block(ii,0,1,6) = jacobianPhoto;
                                    residualsPhoto(ii) = weightedErrorPhoto;
                                    validPixelsPhoto(ii) = 1;
                                }
                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Depth component (Plane ICL like)
//                                    hessian += jacobianDepth.transpose()*jacobianDepth;
//                                    gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                    // Take into account the possible occlusions
                                    jacobiansDepth.block(ii,0,1,6) = jacobianDepth;
                                    residualsDepth(ii) = weightedErrorDepth;
                                    validPixelsDepth(ii) = 1;
                                    depthDiff_[ii] = fabs(depthDiff);
                                }
                            }
                        }
                    }
                }
            //}

            // Compute hessian and gradient
            float h11=0,h12=0,h13=0,h14=0,h15=0,h16=0,h22=0,h23=0,h24=0,h25=0,h26=0,h33=0,h34=0,h35=0,h36=0,h44=0,h45=0,h46=0,h55=0,h56=0,h66=0;
            float g1=0,g2=0,g3=0,g4=0,g5=0,g6=0;

            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
            {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
                for(int i=0; i < imgSize; i++)
                    if(validPixelsPhoto(i))
                    {
                        h11 += jacobiansPhoto(i,0)*jacobiansPhoto(i,0);
                        h12 += jacobiansPhoto(i,0)*jacobiansPhoto(i,1);
                        h13 += jacobiansPhoto(i,0)*jacobiansPhoto(i,2);
                        h14 += jacobiansPhoto(i,0)*jacobiansPhoto(i,3);
                        h15 += jacobiansPhoto(i,0)*jacobiansPhoto(i,4);
                        h16 += jacobiansPhoto(i,0)*jacobiansPhoto(i,5);
                        h22 += jacobiansPhoto(i,1)*jacobiansPhoto(i,1);
                        h23 += jacobiansPhoto(i,1)*jacobiansPhoto(i,2);
                        h24 += jacobiansPhoto(i,1)*jacobiansPhoto(i,3);
                        h25 += jacobiansPhoto(i,1)*jacobiansPhoto(i,4);
                        h26 += jacobiansPhoto(i,1)*jacobiansPhoto(i,5);
                        h33 += jacobiansPhoto(i,2)*jacobiansPhoto(i,2);
                        h34 += jacobiansPhoto(i,2)*jacobiansPhoto(i,3);
                        h35 += jacobiansPhoto(i,2)*jacobiansPhoto(i,4);
                        h36 += jacobiansPhoto(i,2)*jacobiansPhoto(i,5);
                        h44 += jacobiansPhoto(i,3)*jacobiansPhoto(i,3);
                        h45 += jacobiansPhoto(i,3)*jacobiansPhoto(i,4);
                        h46 += jacobiansPhoto(i,3)*jacobiansPhoto(i,5);
                        h55 += jacobiansPhoto(i,4)*jacobiansPhoto(i,4);
                        h56 += jacobiansPhoto(i,4)*jacobiansPhoto(i,5);
                        h66 += jacobiansPhoto(i,5)*jacobiansPhoto(i,5);

                        g1 += jacobiansPhoto(i,0)*residualsPhoto(i);
                        g2 += jacobiansPhoto(i,1)*residualsPhoto(i);
                        g3 += jacobiansPhoto(i,2)*residualsPhoto(i);
                        g4 += jacobiansPhoto(i,3)*residualsPhoto(i);
                        g5 += jacobiansPhoto(i,4)*residualsPhoto(i);
                        g6 += jacobiansPhoto(i,5)*residualsPhoto(i);
                    }
            }
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
            {
#if ENABLE_OPENMP
#pragma omp parallel for reduction (+:h11,h12,h13,h14,h15,h16,h22,h23,h24,h25,h26,h33,h34,h35,h36,h44,h45,h46,h55,h56,h66,g1,g2,g3,g4,g5,g6) // Cannot reduce on Eigen types
#endif
                for(int i=0; i < imgSize; i++)
                    if(validPixelsDepth(i))
                    {
                        h11 += jacobiansDepth(i,0)*jacobiansDepth(i,0);
                        h12 += jacobiansDepth(i,0)*jacobiansDepth(i,1);
                        h13 += jacobiansDepth(i,0)*jacobiansDepth(i,2);
                        h14 += jacobiansDepth(i,0)*jacobiansDepth(i,3);
                        h15 += jacobiansDepth(i,0)*jacobiansDepth(i,4);
                        h16 += jacobiansDepth(i,0)*jacobiansDepth(i,5);
                        h22 += jacobiansDepth(i,1)*jacobiansDepth(i,1);
                        h23 += jacobiansDepth(i,1)*jacobiansDepth(i,2);
                        h24 += jacobiansDepth(i,1)*jacobiansDepth(i,3);
                        h25 += jacobiansDepth(i,1)*jacobiansDepth(i,4);
                        h26 += jacobiansDepth(i,1)*jacobiansDepth(i,5);
                        h33 += jacobiansDepth(i,2)*jacobiansDepth(i,2);
                        h34 += jacobiansDepth(i,2)*jacobiansDepth(i,3);
                        h35 += jacobiansDepth(i,2)*jacobiansDepth(i,4);
                        h36 += jacobiansDepth(i,2)*jacobiansDepth(i,5);
                        h44 += jacobiansDepth(i,3)*jacobiansDepth(i,3);
                        h45 += jacobiansDepth(i,3)*jacobiansDepth(i,4);
                        h46 += jacobiansDepth(i,3)*jacobiansDepth(i,5);
                        h55 += jacobiansDepth(i,4)*jacobiansDepth(i,4);
                        h56 += jacobiansDepth(i,4)*jacobiansDepth(i,5);
                        h66 += jacobiansDepth(i,5)*jacobiansDepth(i,5);

                        g1 += jacobiansDepth(i,0)*residualsDepth(i);
                        g2 += jacobiansDepth(i,1)*residualsDepth(i);
                        g3 += jacobiansDepth(i,2)*residualsDepth(i);
                        g4 += jacobiansDepth(i,3)*residualsDepth(i);
                        g5 += jacobiansDepth(i,4)*residualsDepth(i);
                        g6 += jacobiansDepth(i,5)*residualsDepth(i);
                    }
            }
            // Assign the values for the hessian and gradient
            hessian(0,0) = h11;
            hessian(0,1) = hessian(1,0) = h12;
            hessian(0,2) = hessian(2,0) = h13;
            hessian(0,3) = hessian(3,0) = h14;
            hessian(0,4) = hessian(4,0) = h15;
            hessian(0,5) = hessian(5,0) = h16;
            hessian(1,1) = h22;
            hessian(1,2) = hessian(2,1) = h23;
            hessian(1,3) = hessian(3,1) = h24;
            hessian(1,4) = hessian(4,1) = h25;
            hessian(1,5) = hessian(5,1) = h26;
            hessian(2,2) = h33;
            hessian(2,3) = hessian(3,2) = h34;
            hessian(2,4) = hessian(4,2) = h35;
            hessian(2,5) = hessian(5,2) = h36;
            hessian(3,3) = h44;
            hessian(3,4) = hessian(4,3) = h45;
            hessian(3,5) = hessian(5,3) = h46;
            hessian(4,4) = h55;
            hessian(4,5) = hessian(5,4) = h56;
            hessian(5,5) = h66;

            gradient(0) = g1;
            gradient(1) = g2;
            gradient(2) = g3;
            gradient(3) = g4;
            gradient(4) = g5;
            gradient(5) = g6;
        }
        SSO = (float)numVisiblePixels / imgSize;
//        std::cout << "numVisiblePixels " << numVisiblePixels << " imgSize " << imgSize << " sso " << SSO << std::endl;

        std::vector<float> diffDepth(imgSize);
        int validPt = 0;
        for(int i=0; i < imgSize; i++)
            if(depthDiff_[i] >= 0)
                diffDepth[validPt++] = depthDiff_[i];
        float diffDepthMean, diffDepthStDev;
        calcMeanAndStDev(diffDepth, diffDepthMean, diffDepthStDev);
        std::cout << "diffDepthMean " << diffDepthMean << " diffDepthStDev " << diffDepthStDev << " trans " << poseGuess.block(0,3,3,1).norm() << " sso " << SSO << std::endl;
    }

    /*! Search for the best alignment of a pair of RGB-D frames based on photoconsistency and depthICP.
      * This pose is obtained from an optimization process using Levenberg-Marquardt which is maximizes the photoconsistency and depthCOnsistency
      * between the source and target frames. This process is performed sequentially on a pyramid of image with increasing resolution. */
    void alignFrames(const Eigen::Matrix4f pose_guess = Eigen::Matrix4f::Identity(), costFuncType method = PHOTO_CONSISTENCY, const int occlusion = 0)
    {
        if(occlusion == 2)
        {
            minDepthOutliers = 2*stdDevDepth; // in meters
            thresDepthOutliers = maxDepthOutliers;
        }

        float avResidual_temp, avPhotoResidual_temp, avDepthResidual_temp; // Optimization residuals

        num_iterations.resize(nPyrLevels); // Store the number of iterations
        Eigen::Matrix4f pose_estim_temp, pose_estim = pose_guess;
        for(int pyramidLevel = nPyrLevels-1; pyramidLevel >= 0; pyramidLevel--)
        {
            const int nRows = graySrcPyr[pyramidLevel].rows;
            const int nCols = graySrcPyr[pyramidLevel].cols;
            const int imgSize = nRows*nCols;

            // Make LUT to store the values of the 3D points of the source sphere
            LUT_xyz_sphere.resize(imgSize);
            const float scaleFactor = 1.0/pow(2,pyramidLevel);
            const float fx = cameraMatrix(0,0)*scaleFactor;
            const float fy = cameraMatrix(1,1)*scaleFactor;
            const float ox = cameraMatrix(0,2)*scaleFactor;
            const float oy = cameraMatrix(1,2)*scaleFactor;
            const float inv_fx = 1./fx;
            const float inv_fy = 1./fy;
            for(int r=0;r<nRows;r++)
            {
                for(int c=0;c<nCols;c++)
                {
                    int i = r*nCols + c;
                    LUT_xyz_sphere[i](2) = depthSrcPyr[pyramidLevel].at<float>(r,c); //LUT_xyz_sphere[i](2) = 0.001f*depthSrcPyr[pyramidLevel].at<unsigned short>(r,c);

                    //Compute the 3D coordinates of the pij of the source frame
                    //std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                    //std::cout << depthSrcPyr[pyramidLevel].type() << " LUT_xyz_sphere " << i << " x " << LUT_xyz_sphere[i](2) << " thres " << minDepth << " " << maxDepth << std::endl;
                    if(minDepth < LUT_xyz_sphere[i](2) && LUT_xyz_sphere[i](2) < maxDepth) //Compute the jacobian only for the valid points
                    {
                        LUT_xyz_sphere[i](0) = (c - ox) * LUT_xyz_sphere[i](2) * inv_fx;
                        LUT_xyz_sphere[i](1) = (r - oy) * LUT_xyz_sphere[i](2) * inv_fy;
                    }
                    else
                        LUT_xyz_sphere[i](0) = INVALID_POINT;
                }
            }

            double lambda = 0.01; // Levenberg-Marquardt (LM) lambda
            double step = 10; // Update step
            unsigned LM_maxIters = 1;

            int it = 0, maxIters = 10;
            double tol_residual = 1e-4;
            double tol_update = 1e-4;
            Eigen::Matrix<float,6,1> update_pose; update_pose << 1, 1, 1, 1, 1, 1;
            // double error = errorPhotoICP(pyramidLevel, pose_estim, method);
            double error, new_error;
            if(occlusion == 0)
                error = errorPhotoICP(pyramidLevel, pose_estim, method);
            else if(occlusion == 1)
                error = errorPhotoICP_Occ1(pyramidLevel, pose_estim, method);
            else if(occlusion == 2)
                error = errorPhotoICP_Occ2(pyramidLevel, pose_estim, method);

            double diff_error = error;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
            std::cout << "error2 " << error << std::endl;
            std::cout << "Level " << pyramidLevel << " error " << error << std::endl;
            std::cout << "salient " << vSalientPixels[pyramidLevel].size() << std::endl;
#endif
            while(it < maxIters && update_pose.norm() > tol_update && diff_error > tol_residual) // The LM optimization stops either when the max iterations is reached, or when the alignment converges (error or pose do not change)
            {
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                cv::TickMeter tm; tm.start();
#endif
                // Assign the temporal values for the residuals
                avResidual_temp = avResidual;
                avPhotoResidual_temp = avPhotoResidual;
                avDepthResidual_temp = avDepthResidual;

                //calcHessGrad(pyramidLevel, pose_estim, method);
                if(occlusion == 0)
                    calcHessGrad(pyramidLevel, pose_estim, method);
                else if(occlusion == 1)
                    calcHessGrad_Occ1(pyramidLevel, pose_estim, method);
                else if(occlusion == 2)
                    calcHessGrad_Occ2(pyramidLevel, pose_estim, method);
                else
                    assert(false);

//                assert(hessian.rank() == 6); // Make sure that the problem is observable
                if((hessian + lambda*getDiagonalMatrix(hessian)).rank() != 6)
                {
                    std::cout << "\t The problem is ILL-POSED \n";
                    std::cout << "hessian \n" << hessian << std::endl;
                    std::cout << "gradient \n" << gradient.transpose() << std::endl;
                    relPose = pose_estim;
                    return;
                }

                // Compute the pose update
                update_pose = -hessian.inverse() * gradient;
//                update_pose = -(hessian + lambda*getDiagonalMatrix(hessian)).inverse() * gradient;
                Eigen::Matrix<double,6,1> update_pose_d = update_pose.cast<double>();
                pose_estim_temp = mrpt::poses::CPose3D::exp(mrpt::math::CArrayNumeric<double,6>(update_pose_d)).getHomogeneousMatrixVal().cast<float>() * pose_estim;
//            std::cout << "update_pose \n" << update_pose.transpose() << std::endl;

                // double new_error = errorPhotoICP(pyramidLevel, pose_estim_temp, method);
                if(occlusion == 0)
                    new_error = errorPhotoICP(pyramidLevel, pose_estim_temp, method);
                else if(occlusion == 1)
                    new_error = errorPhotoICP_Occ1(pyramidLevel, pose_estim_temp, method);
                else if(occlusion == 2)
                    new_error = errorPhotoICP_Occ2(pyramidLevel, pose_estim_temp, method);

                diff_error = error - new_error;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                //std::cout << "update_pose \n" << update_pose.transpose() << std::endl;
                std::cout << "diff_error " << diff_error << std::endl;
#endif
                if(diff_error > 0)
                {
//                cout << "pose_estim \n" << pose_estim << "\n pose_estim_temp \n" << pose_estim_temp << endl;
                    lambda /= step;
                    pose_estim = pose_estim_temp;
                    error = new_error;
                    it = it+1;
                }
                else
                {
                    unsigned LM_it = 0;
                    while(LM_it < LM_maxIters && diff_error < 0)
                    {
                        lambda = lambda * step;

                        update_pose = -(hessian + lambda*getDiagonalMatrix(hessian)).inverse() * gradient;
                        Eigen::Matrix<double,6,1> update_pose_d = update_pose.cast<double>();
                        pose_estim_temp = mrpt::poses::CPose3D::exp(mrpt::math::CArrayNumeric<double,6>(update_pose_d)).getHomogeneousMatrixVal().cast<float>() * pose_estim;
                        //new_error = errorPhotoICP(pyramidLevel, pose_estim_temp, method);
                        if(occlusion == 0)
                            new_error = errorPhotoICP(pyramidLevel, pose_estim_temp, method);
                        else if(occlusion == 1)
                            new_error = errorPhotoICP_Occ1(pyramidLevel, pose_estim_temp, method);
                        else if(occlusion == 2)
                            new_error = errorPhotoICP_Occ2(pyramidLevel, pose_estim_temp, method);
                        diff_error = error - new_error;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                        std::cout << "diff_error LM " << diff_error << std::endl;
#endif
                        if(diff_error > 0)
                        {
//                        cout << "pose_estim \n" << pose_estim << "\n pose_estim_temp \n" << pose_estim_temp << endl;
                            pose_estim = pose_estim_temp;
                            error = new_error;
                            it = it+1;
                        }
                        else
                            LM_it = LM_it + 1;
                    }
                }

#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                tm.stop(); std::cout << "Iterations " << it << " time = " << tm.getTimeSec() << " sec." << std::endl;
#endif

                if(visualizeIterations)
                {
                //std::cout << "visualizeIterations\n";
                    if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                    {                        
                        cv::Mat imgDiff = cv::Mat::zeros(nRows,nCols,grayTrgPyr[pyramidLevel].type());
                        cv::absdiff(grayTrgPyr[pyramidLevel], warped_source_grayImage, imgDiff);
                        // std::cout << "imgDiff " << imgDiff.at<float>(20,20) << " " << grayTrgPyr[pyramidLevel].at<float>(20,20) << " " << warped_source_grayImage.at<float>(20,20) << std::endl;
                        // cout << "type " << grayTrgPyr[pyramidLevel].type() << " " << warped_source_grayImage.type() << endl;

//                        cv::imshow("orig", grayTrgPyr[pyramidLevel]);
//                        cv::imshow("src", graySrcPyr[pyramidLevel]);
//                        cv::imshow("optimize::imgDiff", imgDiff);
//                        cv::imshow("warp", warped_source_grayImage);

                        cv::Mat DispImage = cv::Mat(2*grayTrgPyr[pyramidLevel].rows+4, 2*grayTrgPyr[pyramidLevel].cols+4, grayTrgPyr[pyramidLevel].type(), cv::Scalar(255)); // cv::Scalar(100, 100, 200)
                        grayTrgPyr[pyramidLevel].copyTo(DispImage(cv::Rect(0, 0, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        graySrcPyr[pyramidLevel].copyTo(DispImage(cv::Rect(grayTrgPyr[pyramidLevel].cols+4, 0, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        warped_source_grayImage.copyTo(DispImage(cv::Rect(0, grayTrgPyr[pyramidLevel].rows+4, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        imgDiff.copyTo(DispImage(cv::Rect(grayTrgPyr[pyramidLevel].cols+4, grayTrgPyr[pyramidLevel].rows+4, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        //cv::namedWindow("Photoconsistency", cv::WINDOW_AUTOSIZE );// Create a window for display.
                        cv::imshow("Photoconsistency", DispImage);
                    }
                    if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                    {
                        //std::cout << "sizes " << nRows << " " << nCols << " " << "sizes " << depthTrgPyr[pyramidLevel].rows << " " << depthTrgPyr[pyramidLevel].cols << " " << "sizes " << warped_source_depthImage.rows << " " << warped_source_depthImage.cols << " " << grayTrgPyr[pyramidLevel].type() << std::endl;
                        cv::Mat depthDiff = cv::Mat::zeros(nRows,nCols,depthTrgPyr[pyramidLevel].type());
                        cv::absdiff(depthTrgPyr[pyramidLevel], warped_source_depthImage, depthDiff);
                        //cv::imshow("depthDiff", depthDiff);

                        cv::Mat DispImage = cv::Mat(2*grayTrgPyr[pyramidLevel].rows+4, 2*grayTrgPyr[pyramidLevel].cols+4, grayTrgPyr[pyramidLevel].type(), cv::Scalar(255)); // cv::Scalar(100, 100, 200)
                        depthTrgPyr[pyramidLevel].copyTo(DispImage(cv::Rect(0, 0, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        depthSrcPyr[pyramidLevel].copyTo(DispImage(cv::Rect(grayTrgPyr[pyramidLevel].cols+4, 0, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        warped_source_depthImage.copyTo(DispImage(cv::Rect(0, grayTrgPyr[pyramidLevel].rows+4, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        depthDiff.copyTo(DispImage(cv::Rect(grayTrgPyr[pyramidLevel].cols+4, grayTrgPyr[pyramidLevel].rows+4, grayTrgPyr[pyramidLevel].cols, grayTrgPyr[pyramidLevel].rows)));
                        DispImage.convertTo(DispImage, CV_8U, 22.5);

                        //cv::namedWindow("Depth-consistency", cv::WINDOW_AUTOSIZE );// Create a window for display.
                        cv::imshow("Depth-consistency", DispImage);
                    }
                    if(occlusion == 2)
                    {
                        // Draw the segmented features: pixels moving forward and backward and occlusions
                        cv::Mat segmentedSrcImg = colorSrcPyr[pyramidLevel].clone(); // cv::Mat segmentedSrcImg(colorSrcPyr[pyramidLevel],true); // Both should be the same
                        //std::cout << "imgSize  " << imgSize << " nRows*nCols " << nRows << "x" << nCols << " types " << segmentedSrcImg.type() << " " << CV_8UC3 << std::endl;
                        for(unsigned i=0; i < imgSize; i++)
                        {
                            if(mask_dynamic_occlusion.at<uchar>(i) == 255) // Draw in Red (BGR)
                            {
                                segmentedSrcImg.at<cv::Vec3b>(i)[0] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[1] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[2] = 255;
                            }
                            else if(mask_dynamic_occlusion.at<uchar>(i) == 155)
                            {
                                segmentedSrcImg.at<cv::Vec3b>(i)[0] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[1] = 255;
                                segmentedSrcImg.at<cv::Vec3b>(i)[2] = 0;
                            }
                            else if(mask_dynamic_occlusion.at<uchar>(i) == 55)
                            {
                                segmentedSrcImg.at<cv::Vec3b>(i)[0] = 255;
                                segmentedSrcImg.at<cv::Vec3b>(i)[1] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[2] = 0;
                            }
                        }
                        cv::imshow("SegmentedSRC", segmentedSrcImg);
                    }
                    cv::waitKey(0);
                }
            }

            num_iterations[pyramidLevel] = it;
        }

//        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
//            cv::destroyWindow("Photoconsistency");
//        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
//            cv::destroyWindow("Depth-consistency");

//#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
        std::cout << "Iterations: ";
        for(int pyramidLevel = nPyrLevels-1; pyramidLevel >= 0; pyramidLevel--)
            std::cout << num_iterations[pyramidLevel] << " ";
        std::cout << std::endl;
//#endif

        // Assign the temporal values for the residuals
        avResidual = avResidual_temp;
        avPhotoResidual = avPhotoResidual_temp;
        avDepthResidual = avDepthResidual_temp;

        relPose = pose_estim;
    }

    /*! Search for the best alignment of a pair of RGB-D frames based on photoconsistency and depthICP.
      * This pose is obtained from an optimization process using Levenberg-Marquardt which is maximizes the photoconsistency and depthCOnsistency
      * between the source and target frames. This process is performed sequentially on a pyramid of image with increasing resolution.
      * The input parameter occlusion stands for: (0->Regular dense registration, 1->Occlusion1, 2->Occlusion2)
    */
    void alignFrames360(const Eigen::Matrix4f pose_guess = Eigen::Matrix4f::Identity(), costFuncType method = PHOTO_CONSISTENCY, const int occlusion = 0)
    {
//    std::cout << "alignFrames360 " << std::endl;
        double align360_start = pcl::getTime();

//        thresDepthOutliers = maxDepthOutliers;
        thresDepthOutliers = 0.3;

        num_iterations.resize(nPyrLevels); // Store the number of iterations

        double error;
        Eigen::Matrix4f pose_estim_temp, pose_estim = pose_guess;
        for(int pyramidLevel = nPyrLevels-1; pyramidLevel >= 0; pyramidLevel--)
        {
            const int nRows = graySrcPyr[pyramidLevel].rows;
            const int nCols = graySrcPyr[pyramidLevel].cols;
            const int imgSize = nRows*nCols;

            // Mask the joints between the different images to avoid the high gradients that are the result of using auto-shutter for each camera
            int width_sensor = nCols / 8;
            for(int sensor_id = 1; sensor_id < 8; sensor_id++)
            {
                cv::Rect region_of_interest = cv::Rect(sensor_id*width_sensor-1, 0, 2, nRows);
//                cv::Mat image_roi = grayTrgGradXPyr[pyramidLevel](region_of_interest);
//                image_roi = cv::Mat::zeros(image_roi.cols, image_roi.rows, image_roi.type());
                grayTrgGradXPyr[pyramidLevel](region_of_interest) = cv::Mat::zeros(nRows, 2, grayTrgGradXPyr[pyramidLevel].type());
//                grayTrgGradXPyr[pyramidLevel](region_of_interest) = cv::Mat(nRows, 20, grayTrgGradXPyr[pyramidLevel].type(), cv::Scalar(255.f));
                grayTrgGradYPyr[pyramidLevel](region_of_interest) = cv::Mat::zeros(nRows, 2, grayTrgGradYPyr[pyramidLevel].type());
                depthTrgGradXPyr[pyramidLevel](region_of_interest) = cv::Mat::zeros(nRows, 2, depthTrgGradXPyr[pyramidLevel].type());
                depthTrgGradYPyr[pyramidLevel](region_of_interest) = cv::Mat::zeros(nRows, 2, depthTrgGradYPyr[pyramidLevel].type());
            }
//            cv::imshow("test_grad", grayTrgGradXPyr[pyramidLevel]);
//        cv::waitKey(0);

            // Make LUT to store the values of the 3D points of the source sphere
            LUT_xyz_sphere.resize(imgSize);
            const float angle_res = 2*PI/nCols;
            std::vector<float> v_sinTheta(nCols);
            std::vector<float> v_cosTheta(nCols);
            for(int c=0;c<nCols;c++)
            {
                float theta = c*angle_res;
                v_sinTheta[c] = sin(theta);
                v_cosTheta[c] = cos(theta);
            }
            const float half_nRows = 0.5*nRows-0.5;
            for(int r=0;r<nRows;r++)
            {
                float phi = (half_nRows-r)*angle_res;
                float sin_phi = sin(phi);
                float cos_phi = cos(phi);

                for(int c=0;c<nCols;c++)
                {
                    int i = r*nCols + c;

                    //Compute the 3D coordinates of the pij of the source frame
                    float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c);
//                std::cout << " theta " << theta << " phi " << phi << " rc " << r << " " << c << std::endl;
                    if(minDepth < depth1 && depth1 < maxDepth) //Compute the jacobian only for the valid points
                    {
                        LUT_xyz_sphere[i](0) = depth1*sin_phi;
                        LUT_xyz_sphere[i](1) = -depth1*cos_phi*v_sinTheta[c];
                        LUT_xyz_sphere[i](2) = -depth1*cos_phi*v_cosTheta[c];
                    }
                    else
                        LUT_xyz_sphere[i](0) = INVALID_POINT;
                }
            }

            double lambda = 1e0; // Levenberg-Marquardt (LM) lambda
            double step = 5; // Update step
            unsigned LM_maxIters = 1;

            int it = 0, maxIters = 10;
            double tol_residual = 1e-3;
            double tol_update = 1e-4;
            Eigen::Matrix<float,6,1> update_pose; update_pose << 1, 1, 1, 1, 1, 1;
            //error = errorPhotoICP_sphere(pyramidLevel, pose_estim, method);
            if(occlusion == 0)
                error = errorPhotoICP_sphere(pyramidLevel, pose_estim, method);
            else if(occlusion == 1)
                error = errorPhotoICP_sphereOcc1(pyramidLevel, pose_estim, method);
            else if(occlusion == 2)
                error = errorPhotoICP_sphereOcc2(pyramidLevel, pose_estim, method);

            double diff_error = error;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
//            std::cout << "pose_estim \n " << pose_estim << std::endl;
            std::cout << "Level " << pyramidLevel << " error " << error << std::endl;
//            cout << "salient " << vSalientPixels[pyramidLevel].size() << endl;
#endif
            while(it < maxIters && update_pose.norm() > tol_update && diff_error > tol_residual) // The LM optimization stops either when the max iterations is reached, or when the alignment converges (error or pose do not change)
            {
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                cv::TickMeter tm;tm.start();
#endif
////                std::cout << "calcHessianAndGradient_sphere " << std::endl;
//                calcHessGrad_sphere(pyramidLevel, pose_estim, method);
////            std::cout << "hessian \n" << hessian << std::endl;
////            std::cout << "gradient \n" << gradient.transpose() << std::endl;
////                assert(hessian.rank() == 6); // Make sure that the problem is observable

                if(occlusion == 0)
                    calcHessGrad_sphere(pyramidLevel, pose_estim, method);
                else if(occlusion == 1)
                    calcHessGrad_sphereOcc1(pyramidLevel, pose_estim, method);
                else if(occlusion == 2)
                    calcHessGrad_sphereOcc2(pyramidLevel, pose_estim, method);
                else
                    assert(false);

                if(visualizeIterations)
                {
                    if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                    {
                        // Draw the segmented features: pixels moving forward and backward and occlusions
                        cv::Mat segmentedSrcImg = colorSrcPyr[pyramidLevel].clone();
                        //std::cout << "imgSize  " << imgSize << " nRows*nCols " << nRows << "x" << nCols << " types " << segmentedSrcImg.type() << " " << CV_8UC3 << std::endl;
                        for(unsigned i=0; i < imgSize; i++)
                        {
                            if(mask_dynamic_occlusion.at<uchar>(i) == 255) // Draw in Red (BGR)
                            {
                                segmentedSrcImg.at<cv::Vec3b>(i)[0] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[1] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[2] = 255;
                            }
                            else if(mask_dynamic_occlusion.at<uchar>(i) == 155)
                            {
                                segmentedSrcImg.at<cv::Vec3b>(i)[0] = 0;
                                segmentedSrcImg.at<cv::Vec3b>(i)[1] = 255;
                                segmentedSrcImg.at<cv::Vec3b>(i)[2] = 0;
                            }
    //                        else if(mask_dynamic_occlusion.at<uchar>(i) == 55)
    //                        {
    //                            segmentedSrcImg.at<cv::Vec3b>(i)[0] = 255;
    //                            segmentedSrcImg.at<cv::Vec3b>(i)[1] = 0;
    //                            segmentedSrcImg.at<cv::Vec3b>(i)[2] = 0;
    //                        }
                        }
                        cv::imshow("SegmentedSRC", segmentedSrcImg);

                        cv::imshow("trg", grayTrgPyr[pyramidLevel]);
                        cv::imshow("src", graySrcPyr[pyramidLevel]);

                        cv::Mat imgDiff = cv::Mat::zeros(nRows,nCols,grayTrgPyr[pyramidLevel].type());
                        cv::absdiff(grayTrgPyr[pyramidLevel], warped_source_grayImage, imgDiff);
    //                std::cout << "imgDiff " << imgDiff.at<float>(20,20) << " " << grayTrgPyr[pyramidLevel].at<float>(20,20) << " " << warped_source_grayImage.at<float>(20,20) << std::endl;
                        cv::imshow("intensityDiff", imgDiff);
                        cv::imshow("warp", warped_source_grayImage);
//                    cv::Mat imgDiffsave;
//                    imgDiff.convertTo(imgDiffsave, CV_8UC1, 255);
//                    cv::imwrite( mrpt::format("/home/edu/photoDiff.png"), imgDiffsave);
                    }
                    if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                    {
                        cv::Mat depthDiff = cv::Mat::zeros(nRows,nCols,depthTrgPyr[pyramidLevel].type());
                        cv::absdiff(depthTrgPyr[pyramidLevel], warped_source_depthImage, depthDiff);
                        cv::imshow("depthDiff", depthDiff);
                    }
                    cv::waitKey(0);
                }

                if((hessian + lambda*getDiagonalMatrix(hessian)).rank() != 6)
                {
                    std::cout << "\t The problem is ILL-POSED \n";
//                    std::cout << "hessian \n" << hessian << std::endl;
//                    std::cout << "gradient \n" << gradient.transpose() << std::endl;
                    relPose = pose_estim;
                    avResidual = 0;
                    return;
                }

                // Compute the pose update
                update_pose = -hessian.inverse() * gradient;
//                update_pose = -(hessian + lambda*getDiagonalMatrix(hessian)).inverse() * gradient;
                Eigen::Matrix<double,6,1> update_pose_d = update_pose.cast<double>();
//                update_pose_d.block(0,0,3,1) = -update_pose_d.block(0,0,3,1);
                pose_estim_temp = mrpt::poses::CPose3D::exp(mrpt::math::CArrayNumeric<double,6>(update_pose_d), true).getHomogeneousMatrixVal().cast<float>() * pose_estim;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                std::cout << "update_pose \n" << update_pose.transpose() << " \n ";// << mrpt::poses::CPose3D::exp(mrpt::math::CArrayNumeric<double,6>(update_pose_d), true).getHomogeneousMatrixVal().cast<float>() << std::endl;
#endif

                //double new_error = errorPhotoICP_sphere(pyramidLevel, pose_estim_temp, method);
                double new_error;
                if(occlusion == 0)
                    new_error = errorPhotoICP_sphere(pyramidLevel, pose_estim_temp, method);
                else if(occlusion == 1)
                    new_error = errorPhotoICP_sphereOcc1(pyramidLevel, pose_estim_temp, method);
                else if(occlusion == 2)
                    new_error = errorPhotoICP_sphereOcc2(pyramidLevel, pose_estim_temp, method);

                diff_error = error - new_error;
#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                cout << "diff_error " << diff_error << endl;
#endif
                if(diff_error > tol_residual)
                {
//                cout << "pose_estim \n" << pose_estim << "\n pose_estim_temp \n" << pose_estim_temp << endl;
                    lambda /= step;
                    pose_estim = pose_estim_temp;
                    error = new_error;
                    it = it+1;
                }
//                else
//                {
//                    unsigned LM_it = 0;
//                    while(LM_it < LM_maxIters && diff_error < 0)
//                    {
//                        lambda = lambda * step;

////                        update_pose = hessian.inverse() * gradient;
//                        update_pose = -(hessian + lambda*getDiagonalMatrix(hessian)).inverse() * gradient;
//                        Eigen::Matrix<double,6,1> update_pose_d = update_pose.cast<double>();
//                        pose_estim_temp = mrpt::poses::CPose3D::exp(mrpt::math::CArrayNumeric<double,6>(update_pose_d)).getHomogeneousMatrixVal().cast<float>() * pose_estim;
//                        double new_error = errorPhotoICP_sphere(pyramidLevel, pose_estim_temp, method);
//                        diff_error = error - new_error;
//#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
//                        cout << "diff_error LM " << diff_error << endl;
//#endif
//                        if(diff_error > tol_residual)
//                        {
//                        cout << "pose_estim \n" << pose_estim << "\n pose_estim_temp \n" << pose_estim_temp << endl;
//                            pose_estim = pose_estim_temp;
//                            error = new_error;
//                            it = it+1;
//                        }
//                        LM_it = LM_it + 1;
//                    }
//                }

#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
                tm.stop(); std::cout << "Iterations " << it << " time = " << tm.getTimeSec() << " sec." << std::endl;
#endif

//                if(visualizeIterations)
//                {
//                    cv::Mat imgDiff = cv::Mat::zeros(nRows,nCols,grayTrgPyr[pyramidLevel].type());
//                    cv::absdiff(grayTrgPyr[pyramidLevel], warped_source_grayImage, imgDiff);
////                std::cout << "imgDiff " << imgDiff.at<float>(20,20) << " " << grayTrgPyr[pyramidLevel].at<float>(20,20) << " " << warped_source_grayImage.at<float>(20,20) << std::endl;
//                    cv::imshow("optimize::imgDiff", imgDiff);

//                    cv::imshow("orig", grayTrgPyr[pyramidLevel]);
//                    cv::imshow("warp", warped_source_grayImage);

//                    cv::Mat depthDiff = cv::Mat::zeros(nRows,nCols,depthTrgPyr[pyramidLevel].type());
//                    cv::absdiff(depthTrgPyr[pyramidLevel], warped_source_depthImage, depthDiff);
//                    cv::imshow("depthDiff", depthDiff);

//                    cv::waitKey(0);
//                }
            }

            num_iterations[pyramidLevel] = it;
        }

//#if ENABLE_PRINT_CONSOLE_OPTIMIZATION_PROGRESS
        double align360_end = pcl::getTime();
        std::cout << "Dense alignment 360 took " << double (align360_end - align360_start) << " iterations: "; //<< std::endl;
        for(int pyramidLevel = nPyrLevels-1; pyramidLevel >= 0; pyramidLevel--)
            std::cout << num_iterations[pyramidLevel] << " ";
        std::cout << std::endl;
//#endif

        relPose = pose_estim;
    }

    /*! Calculate entropy of the planar matching. This is the differential entropy of a multivariate normal distribution given
      by the matched planes. This is calculated with the formula used in the paper ["Dense Visual SLAM for RGB-D Cameras",
      by C. Kerl et al., in IROS 2013]. */
    float calcEntropy()
    {
        Eigen::Matrix<float,6,6> covarianceM = hessian.inverse();
        std::cout << covarianceM.determinant() << " " << hessian.determinant() << " " << log(2*PI) << endl;
        float DOF = 6;
        float entropy = 0.5 * ( DOF * (1 + log(2*PI)) + log(covarianceM.determinant()) );

        return entropy;
    }

    /*! Align depth frames applying ICP in different pyramid scales. */
    double alignPyramidICP(Eigen::Matrix4f poseGuess)
    {
//        vector<pcl::PointCloud<PointT> > pyrCloudSrc(nPyrLevels);
//        vector<pcl::PointCloud<PointT> > pyrCloudTrg(nPyrLevels);

        // ICP alignement
        pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ,pcl::PointXYZ> icp;

        icp.setMaxCorrespondenceDistance (0.3);
        icp.setMaximumIterations (10);
        icp.setTransformationEpsilon (1e-6);
        //  icp.setEuclideanFitnessEpsilon (1);
        icp.setRANSACOutlierRejectionThreshold (0.1);

        for(int pyramidLevel = nPyrLevels-1; pyramidLevel >= 0; pyramidLevel--)
        {
            const int height = depthSrcPyr[pyramidLevel].rows;
            const int width = depthSrcPyr[pyramidLevel].cols;

            const float res_factor_VGA = width / 640.0;
            const float focal_length = 525 * res_factor_VGA;
            const float inv_fx = 1.f/focal_length;
            const float inv_fy = 1.f/focal_length;
            const float ox = width/2 - 0.5;
            const float oy = height/2 - 0.5;

            pcl::PointCloud<pcl::PointXYZ>::Ptr srcCloudPtr(new pcl::PointCloud<pcl::PointXYZ>());
            srcCloudPtr->height = height;
            srcCloudPtr->width = width;
            srcCloudPtr->is_dense = false;
            srcCloudPtr->points.resize(height*width);

            pcl::PointCloud<pcl::PointXYZ>::Ptr trgCloudPtr(new pcl::PointCloud<pcl::PointXYZ>());
            trgCloudPtr->height = height;
            trgCloudPtr->width = width;
            trgCloudPtr->is_dense = false;
            trgCloudPtr->points.resize(height*width);

#if ENABLE_OPENMP
#pragma omp parallel for
#endif
            for( int y = 0; y < height; y++ )
            {
                for( int x = 0; x < width; x++ )
                {
                    float z = depthSrcPyr[pyramidLevel].at<float>(y,x); //convert from milimeters to meters
                    //std::cout << "Build " << z << std::endl;
                    //if(z>0 && z>=minDepth && z<=maxDepth) //If the point has valid depth information assign the 3D point to the point cloud
                    if(z>=minDepth && z<=maxDepth) //If the point has valid depth information assign the 3D point to the point cloud
                    {
                        srcCloudPtr->points[width*y+x].x = (x - ox) * z * inv_fx;
                        srcCloudPtr->points[width*y+x].y = (y - oy) * z * inv_fy;
                        srcCloudPtr->points[width*y+x].z = z;
                    }
                    else //else, assign a NAN value
                    {
                        srcCloudPtr->points[width*y+x].x = std::numeric_limits<float>::quiet_NaN ();
                        srcCloudPtr->points[width*y+x].y = std::numeric_limits<float>::quiet_NaN ();
                        srcCloudPtr->points[width*y+x].z = std::numeric_limits<float>::quiet_NaN ();
                    }

                    z = depthTrgPyr[pyramidLevel].at<float>(y,x); //convert from milimeters to meters
                    //std::cout << "Build " << z << std::endl;
                    //if(z>0 && z>=minDepth && z<=maxDepth) //If the point has valid depth information assign the 3D point to the point cloud
                    if(z>=minDepth && z<=maxDepth) //If the point has valid depth information assign the 3D point to the point cloud
                    {
                        trgCloudPtr->points[width*y+x].x = (x - ox) * z * inv_fx;
                        trgCloudPtr->points[width*y+x].y = (y - oy) * z * inv_fy;
                        trgCloudPtr->points[width*y+x].z = z;
                    }
                    else //else, assign a NAN value
                    {
                        trgCloudPtr->points[width*y+x].x = std::numeric_limits<float>::quiet_NaN ();
                        trgCloudPtr->points[width*y+x].y = std::numeric_limits<float>::quiet_NaN ();
                        trgCloudPtr->points[width*y+x].z = std::numeric_limits<float>::quiet_NaN ();
                    }
                }
            }

            // Remove NaN points
            pcl::PointCloud<pcl::PointXYZ>::Ptr srcCloudPtr_(new pcl::PointCloud<pcl::PointXYZ>());
            pcl::PointCloud<pcl::PointXYZ>::Ptr trgCloudPtr_(new pcl::PointCloud<pcl::PointXYZ>());
            std::vector<int> nan_indices;
            pcl::removeNaNFromPointCloud(*srcCloudPtr,*srcCloudPtr_,nan_indices);
            //std::cout << " pts " << srcCloudPtr->size() << " pts " << srcCloudPtr_->size() << std::endl;
            pcl::removeNaNFromPointCloud(*trgCloudPtr,*trgCloudPtr_,nan_indices);

            // ICP registration:
            icp.setInputSource(srcCloudPtr_);
            icp.setInputTarget(trgCloudPtr_);
            pcl::PointCloud<pcl::PointXYZ>::Ptr alignedICP(new pcl::PointCloud<pcl::PointXYZ>);
            icp.align(*alignedICP, poseGuess);
            poseGuess = icp.getFinalTransformation();

            // std::cout << "has converged:" << icp.hasConverged() << " iterations " << icp.countIterations() << " score: " << icp.getFitnessScore() << std::endl;
            //std::cout << pyramidLevel << " PyrICP has converged: " << icp.hasConverged() << " score: " << icp.getFitnessScore() << std::endl;
        }
        relPose = poseGuess;
    }


    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    double calcPhotoICPError_robot(const int &pyramidLevel,
                                   const Eigen::Matrix4f poseGuess,
                                   const Eigen::Matrix4f &poseCamRobot,
                                   costFuncType method = PHOTO_CONSISTENCY)
    {
        double error2 = 0.0; // Squared error

        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const float scaleFactor = 1.0/pow(2,pyramidLevel);
        const float fx = cameraMatrix(0,0)*scaleFactor;
        const float fy = cameraMatrix(1,1)*scaleFactor;
        const float ox = cameraMatrix(0,2)*scaleFactor;
        const float oy = cameraMatrix(1,2)*scaleFactor;
        const float inv_fx = 1./fx;
        const float inv_fy = 1./fy;

        const Eigen::Matrix4f poseCamRobot_inv = poseCamRobot.inverse();
        const Eigen::Matrix4f relPoseCam = poseCamRobot_inv*poseGuess*poseCamRobot;

        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        double stdDevPhoto_inv = 1./stdDevPhoto;
        double stdDevDepth_inv = 1./stdDevDepth;

        if(bUseSalientPixels)
        {
    #if ENABLE_OPENMP
    #pragma omp parallel for reduction (+:error2)
    #endif
            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
            {
                int r = vSalientPixels[pyramidLevel][i] / nCols;
                int c = vSalientPixels[pyramidLevel][i] % nCols;
                //Compute the 3D coordinates of the pij of the source frame
                Eigen::Vector4f point3D;
                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                {
                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
                    point3D(1)=(r - oy) * point3D(2) * inv_fy;
                    point3D(3)=1;

                    //Transform the 3D point using the transformation matrix Rt
                    Eigen::Vector4f transformedPoint3D = relPoseCam*point3D;

                    //Project the 3D point to the 2D plane
                    double inv_transformedPz = 1.0/transformedPoint3D(2);
                    double transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);

                    //Asign the intensity value to the warped image and compute the difference between the transformed
                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            //Obtain the pixel values that will be used to compute the pixel residual
                            float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                            float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
                            float weightedErrorPhoto = weight_photo * photoDiff;
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                            error2 += weightedErrorPhoto*weightedErrorPhoto;
                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                float depthDiff = depth2 - depth1;
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*depth1;
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                float weightedErrorDepth = weight_depth * depthDiff;
                                error2 += weightedErrorDepth*weightedErrorDepth;
                            }
                        }
                    }
                }
            }
        }
        else
        {
    std::cout << "calcPhotoICPError_robot error2 " << error2 << std::endl;
    #if ENABLE_OPENMP
    #pragma omp parallel for reduction (+:error2)
    #endif
            for (int r=0;r<nRows;r++)
            {
                for (int c=0;c<nCols;c++)
                {
                    // int i = nCols*r+c; //vector index for sorting salient pixels

                    //Compute the 3D coordinates of the pij of the source frame
                    Eigen::Vector4f point3D;
                    point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                    {
                        point3D(0)=(c - ox) * point3D(2) * inv_fx;
                        point3D(1)=(r - oy) * point3D(2) * inv_fy;
                        point3D(3)=1;

                        //Transform the 3D point using the transformation matrix Rt
                        Eigen::Vector4f transformedPoint3D = relPoseCam*point3D;

                        //Project the 3D point to the 2D plane
                        double inv_transformedPz = 1.0/transformedPoint3D(2);
                        double transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the pixel values that will be used to compute the pixel residual
                                float pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                float pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv; // Apply M-estimator weighting
                                float weightedErrorPhoto = weight_photo * photoDiff;
    //                            if(photoDiff2 > varianceRegularization)
    //                            {
    ////                                float photoDiff2_norm = sqrt(photoDiff2);
    ////                                double weight = sqrt(2*stdDevReg*photoDiff2_norm-varianceRegularization) / photoDiff2_norm;
    //                                photoDiff2 = 2*stdDevReg*sqrt(photoDiff2)-varianceRegularization;
    //                            }
                                error2 += weightedErrorPhoto*weightedErrorPhoto;
    //                                std::cout << "error2 " << error2 << " weightedErrorPhoto " << weightedErrorPhoto << " " << weight_photo << " " << photoDiff << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                float depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    //Obtain the depth values that will be used to the compute the depth residual
                                    float depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    float weightedErrorDepth = weight_depth * depthDiff;
                                    error2 += weightedErrorDepth*weightedErrorDepth;
                                }
                            }
    //                            std::cout << " error2 " << error2 << std::endl;
                        }
                    }
                }
            }
        }

        return error2;
    }


    /*! Compute the residuals and the jacobians for each iteration of the dense alignemnt method to build the Hessian and Gradient.
        This is done following the work in:
        Direct iterative closest point for real-time visual odometry. Tykkala, Tommi and Audras, Cédric and Comport, Andrew I.
        in Computer Vision Workshops (ICCV Workshops), 2011. */
    void calcHessianGradient_robot( const int &pyramidLevel,
                                    const Eigen::Matrix4f poseGuess, // The relative pose of the robot between the two frames
                                    const Eigen::Matrix4f &poseCamRobot, // The pose of the camera wrt to the Robot (fixed beforehand through calibration) // Maybe calibration can be computed at the same time
                                    costFuncType method = PHOTO_CONSISTENCY)
    {
        const int nRows = graySrcPyr[pyramidLevel].rows;
        const int nCols = graySrcPyr[pyramidLevel].cols;

        const double scaleFactor = 1.0/pow(2,pyramidLevel);
        const double fx = cameraMatrix(0,0)*scaleFactor;
        const double fy = cameraMatrix(1,1)*scaleFactor;
        const double ox = cameraMatrix(0,2)*scaleFactor;
        const double oy = cameraMatrix(1,2)*scaleFactor;
        const double inv_fx = 1./fx;
        const double inv_fy = 1./fy;

        hessian = Eigen::Matrix<float,6,6>::Zero();
        gradient = Eigen::Matrix<float,6,1>::Zero();

        double weight_photo = 1./stdDevPhoto, weight_depth = 1./stdDevDepth; // The weights should include the Huber estimator plus the information of the covariance
        //        depthComponentGain = cv::mean(target_grayImg).val[0]/cv::mean(target_depthImg).val[0];
        double stdDevPhoto_inv = 1./stdDevPhoto;
        double stdDevDepth_inv = 1./stdDevDepth;

        const Eigen::Matrix4f poseCamRobot_inv = poseCamRobot.inverse();
        const Eigen::Matrix4f relPoseCam2 = poseGuess*poseCamRobot;

    //    std::cout << "poseCamRobot \n" << poseCamRobot << std::endl;

        if(visualizeIterations)
        {
            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_grayImage = cv::Mat::zeros(nRows,nCols,graySrcPyr[pyramidLevel].type());
            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                warped_source_depthImage = cv::Mat::zeros(nRows,nCols,depthSrcPyr[pyramidLevel].type());
    //            cout << "type__ " << grayTrgPyr[pyramidLevel].type() << " " << warped_source_grayImage.type() << endl;
        }

        if(bUseSalientPixels)
        {
    //#if ENABLE_OPENMP
    //#pragma omp parallel for
    //#endif
            for (int i=0; i < vSalientPixels[pyramidLevel].size(); i++)
            {
                int r = vSalientPixels[pyramidLevel][i] / nCols;
                int c = vSalientPixels[pyramidLevel][i] % nCols;
                // int i = nCols*r+c; //vector index

                //Compute the 3D coordinates of the pij of the source frame
                Eigen::Vector4f point3D;
                point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                {
                    point3D(0)=(c - ox) * point3D(2) * inv_fx;
                    point3D(1)=(r - oy) * point3D(2) * inv_fy;
                    point3D(3)=1;

                    //Transform the 3D point using the transformation matrix Rt
    //                    Eigen::Vector4f point3D_robot = poseCamRobot*point3D;
                    Eigen::Vector4f point3D_robot2 = relPoseCam2*point3D;
                    Eigen::Vector4f transformedPoint3D = poseCamRobot_inv*point3D_robot2;
    //                std::cout << "transformedPoint3D " << transformedPoint3D.transpose() << std::endl;

                    //Project the 3D point to the 2D plane
                    double inv_transformedPz = 1.0/transformedPoint3D(2);
                    double transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                    transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                    transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                    int transformed_r_int = round(transformed_r);
                    int transformed_c_int = round(transformed_c);
    //                std::cout << "transformed_r_int " << transformed_r_int << " transformed_c_int " << transformed_c_int << std::endl;

                    //Asign the intensity value to the warped image and compute the difference between the transformed
                    //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                    if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                        (transformed_c_int>=0 && transformed_c_int < nCols) )
                    {
                        //Compute the pixel jacobian
                        Eigen::Matrix<float,3,6> jacobianT36;
                        jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
                        Eigen::Vector3f rotatedPoint3D = point3D_robot2.block(0,0,3,1);// - poseGuess.block(0,3,3,1); // TODO: make more efficient
                        jacobianT36.block(0,3,3,3) = -skew( rotatedPoint3D );
                        jacobianT36 = poseCamRobot_inv.block(0,0,3,3) * jacobianT36;

                        Eigen::Matrix<float,2,3> jacobianProj23;
                        //Derivative with respect to x
                        jacobianProj23(0,0)=fx*inv_transformedPz;
                        jacobianProj23(1,0)=0;
                        //Derivative with respect to y
                        jacobianProj23(0,1)=0;
                        jacobianProj23(1,1)=fy*inv_transformedPz;
                        //Derivative with respect to z
                        jacobianProj23(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
                        jacobianProj23(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

                        Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;


                        float pixel1, pixel2, depth1, depth2;
                        double weightedErrorPhoto, weightedErrorDepth;
                        Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                        if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                            if(visualizeIterations)
                                warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = pixel1;

                            //Obtain the pixel values that will be used to compute the pixel residual
                            pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                            float photoDiff = pixel2 - pixel1;
                            weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                            weightedErrorPhoto = weight_photo * photoDiff;

                            //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                            Eigen::Matrix<float,1,2> target_imgGradient;
                            target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                            target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;
                        }
                        if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                        {
                            //Obtain the depth values that will be used to the compute the depth residual
                            depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                            if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                            {
                                depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                if(visualizeIterations)
                                    warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = depth1;

                                float depthDiff = depth2 - depth1;
                                //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                float stdDev_depth1 = stdDevDepth*depth1;
                                weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                weightedErrorDepth = weight_depth * depthDiff;

                                //Depth jacobian:
                                //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                Eigen::Matrix<float,1,2> target_depthGradient;
                                target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);

                                Eigen::Matrix<float,1,6> jacobianRt_z;
                                jacobianT36.block(2,0,1,6);
                                jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
                                // cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;
                            }
                        }

                        //Assign the pixel residual and jacobian to its corresponding row
    //#if ENABLE_OPENMP
    //#pragma omp critical
    //#endif
                        {
                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                // Photometric component
                                //                                hessian += jacobianPhoto.transpose()*jacobianPhoto / varPhoto;
                                hessian += jacobianPhoto.transpose()*jacobianPhoto;
                                gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                {
                                    // Depth component (Plane ICL like)
                                    hessian += jacobianDepth.transpose()*jacobianDepth;
                                    gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                }
                        }
                    }
                }
            }
        }
        else // Use all points
        {
    //#if ENABLE_OPENMP
    //#pragma omp parallel for
    //#endif
            for (int r=0;r<nRows;r++)
            {
                for (int c=0;c<nCols;c++)
                {
                    //                int i = nCols*r+c; //vector index

                    //Compute the 3D coordinates of the pij of the source frame
                    Eigen::Vector4f point3D;
                    point3D(2) = depthSrcPyr[pyramidLevel].at<float>(r,c);
                    if(minDepth < point3D(2) && point3D(2) < maxDepth) //Compute the jacobian only for the valid points
                    {
                        point3D(0)=(c - ox) * point3D(2) * inv_fx;
                        point3D(1)=(r - oy) * point3D(2) * inv_fy;
                        point3D(3)=1;

                        //Transform the 3D point using the transformation matrix Rt
                        Eigen::Vector4f point3D_robot = poseCamRobot*point3D;
                        Eigen::Vector4f point3D_robot2 = poseGuess*point3D_robot;
                        Eigen::Vector4f transformedPoint3D = poseCamRobot_inv*point3D_robot2;
                        //                    Eigen::Vector3f  transformedPoint3D = poseGuess.block(0,0,3,3)*point3D;
    //                    std::cout << "transformedPoint3D " << transformedPoint3D.transpose() << std::endl;

                        //Project the 3D point to the 2D plane
                        double inv_transformedPz = 1.0/transformedPoint3D(2);
                        double transformed_r,transformed_c; // 2D coordinates of the transformed pixel(r,c) of frame 1
                        transformed_c = (transformedPoint3D(0) * fx)*inv_transformedPz + ox; //transformed x (2D)
                        transformed_r = (transformedPoint3D(1) * fy)*inv_transformedPz + oy; //transformed y (2D)
                        int transformed_r_int = round(transformed_r);
                        int transformed_c_int = round(transformed_c);
    //                std::cout << "transformed_r_int " << transformed_r_int << " transformed_c_int " << transformed_c_int << std::endl;

                        //Asign the intensity value to the warped image and compute the difference between the transformed
                        //pixel of the source frame and the corresponding pixel of target frame. Compute the error function
                        if( (transformed_r_int>=0 && transformed_r_int < nRows) &&
                            (transformed_c_int>=0 && transformed_c_int < nCols) )
                        {
                            //Compute the pixel jacobian
                            Eigen::Matrix<float,3,6> jacobianT36;
                            jacobianT36.block(0,0,3,3) = Eigen::Matrix<float,3,3>::Identity();
                            Eigen::Vector3f rotatedPoint3D = point3D_robot2.block(0,0,3,1);// - poseGuess.block(0,3,3,1); // TODO: make more efficient
                            jacobianT36.block(0,3,3,3) = -skew( rotatedPoint3D );
                            jacobianT36 = poseCamRobot_inv.block(0,0,3,3) * jacobianT36;

                            Eigen::Matrix<float,2,3> jacobianProj23;
                            //Derivative with respect to x
                            jacobianProj23(0,0)=fx*inv_transformedPz;
                            jacobianProj23(1,0)=0;
                            //Derivative with respect to y
                            jacobianProj23(0,1)=0;
                            jacobianProj23(1,1)=fy*inv_transformedPz;
                            //Derivative with respect to z
                            jacobianProj23(0,2)=-fx*transformedPoint3D(0)*inv_transformedPz*inv_transformedPz;
                            jacobianProj23(1,2)=-fy*transformedPoint3D(1)*inv_transformedPz*inv_transformedPz;

                            Eigen::Matrix<float,2,6> jacobianWarpRt = jacobianProj23 * jacobianT36;


                            float pixel1, pixel2, depth1, depth2;
                            double weightedErrorPhoto, weightedErrorDepth;
                            Eigen::Matrix<float,1,6> jacobianPhoto, jacobianDepth;

                            if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                pixel1 = graySrcPyr[pyramidLevel].at<float>(r,c); // Intensity value of the pixel(r,c) of source frame
                                if(visualizeIterations)
                                    warped_source_grayImage.at<float>(transformed_r_int,transformed_c_int) = pixel1;

                                Eigen::Matrix<float,1,2> target_imgGradient;
                                target_imgGradient(0,0) = grayTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);//(i);
                                target_imgGradient(0,1) = grayTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(fabs(target_imgGradient(0,0)) < thresSaliencyIntensity && fabs(target_imgGradient(0,1)) < thresSaliencyIntensity)
                                    continue;

                                //Obtain the pixel values that will be used to compute the pixel residual
                                pixel2 = grayTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int); //Intensity value of the pixel(r,c) of the warped target (reference) frame
                                float photoDiff = pixel2 - pixel1;
                                weight_photo = weightHuber(photoDiff,stdDevPhoto)*stdDevPhoto_inv;
                                weightedErrorPhoto = weight_photo * photoDiff;

                                //Apply the chain rule to compound the image gradients with the projective+RigidTransform jacobians
                                jacobianPhoto = weight_photo * target_imgGradient*jacobianWarpRt;

    //                            std::cout << "weight_photo " << weight_photo << " target_imgGradient " << target_imgGradient << "\njacobianWarpRt\n" << jacobianWarpRt << std::endl;
    //                            std::cout << "jacobianPhoto " << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;
    //                                std::cout << "hessian " << hessian << std::endl;
                            }
                            if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                            {
                                //Obtain the depth values that will be used to the compute the depth residual
                                depth2 = depthTrgPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                    depth1 = depthSrcPyr[pyramidLevel].at<float>(r,c); // Depth value of the pixel(r,c) of the warped frame 1 (target)
                                {
                                    if(visualizeIterations)
                                        warped_source_depthImage.at<float>(transformed_r_int,transformed_c_int) = depth1;

                                    Eigen::Matrix<float,1,2> target_depthGradient;
                                    target_depthGradient(0,0) = depthTrgGradXPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    target_depthGradient(0,1) = depthTrgGradYPyr[pyramidLevel].at<float>(transformed_r_int,transformed_c_int);
                                    if(fabs(target_depthGradient(0,0)) < thresSaliencyDepth && fabs(target_depthGradient(0,1)) < thresSaliencyDepth)
                                        continue;

                                    float depthDiff = depth2 - depth1;
                                    //weight_depth = weightHuber(depthDiff,stdDevDepth)*stdDevPhoto_inv;
                                    float stdDev_depth1 = stdDevDepth*depth1;
                                    weight_depth = weightHuber(depthDiff,stdDev_depth1)/stdDev_depth1;
                                    weightedErrorDepth = weight_depth * depthDiff;

                                    //Depth jacobian:
                                    //Apply the chain rule to compound the depth gradients with the projective+RigidTransform jacobians
                                    Eigen::Matrix<float,1,6> jacobianRt_z;
                                    jacobianT36.block(2,0,1,6);
                                    jacobianDepth = weight_depth * (target_depthGradient*jacobianWarpRt-jacobianRt_z);
                                    // cout << "jacobianDepth " << jacobianDepth << " weightedErrorDepth " << weightedErrorDepth << endl;
                                }
                            }

                            //Assign the pixel residual and jacobian to its corresponding row
    //    #if ENABLE_OPENMP
    //    #pragma omp critical
    //    #endif
                            {
                                if(method == PHOTO_CONSISTENCY || method == PHOTO_DEPTH)
                                {
                                    // Photometric component
    //                                    std::cout << "c " << c << " r " << r << std::endl;
    //                                    std::cout << "hessian \n" << hessian << std::endl;
                                    hessian += jacobianPhoto.transpose()*jacobianPhoto;
                                    gradient += jacobianPhoto.transpose()*weightedErrorPhoto;
    //                                    std::cout << "jacobianPhoto \n" << jacobianPhoto << " weightedErrorPhoto " << weightedErrorPhoto << std::endl;
    //                                    std::cout << "hessian \n" << hessian << std::endl;
                                }
                                if(method == DEPTH_CONSISTENCY || method == PHOTO_DEPTH)
                                    if(std::isfinite(depth2)) // Make sure this point has depth (not a NaN)
                                    {
                                        // Depth component (Plane ICL like)
                                        hessian += jacobianDepth.transpose()*jacobianDepth;
                                        gradient += jacobianDepth.transpose()*weightedErrorDepth;
                                    }
                            }
                        }
                    }
                }
            }
        }
    }

};

#endif

------------------------------------------
- OpenNI2_Grabber -
------------------------------------------

Author: Eduardo Fernandez-Moral

*A more complete and updated version of this documentation can be found in 'doc/html/index.html'

This project contains the basic functionality to acquire images from a RGB-D sensor using OpenNI2, and to perform some basic operations. Functions to serialize (load/save) such images, to undistort the depth images, or building point clouds are also provided. Two applications are provided:

** - OpenNIGrabber reads the RGB-D data stream from a sensor or a file and displays the video sequence.

** - CloudGrabber reads the RGB-D data stream from a sensor and displays the point cloud corresponding to the current frame.


--------- Project Files Structure ---------

- frameRGBD
This folder contains the file FrameRGBD which is the basic entity in this project. This class stores the RGB and depth images in opencv structures, plus some other relevant information like the time-stamp. Other classes are provided to undistort the depth image with an intrinsic model (FrameRGBD_undistort), to build a point cloud representation from an RGB-D image (CloudRGBD), to serialize the images (SerializeFrameRGBD) and to downsample the images (DownsampleRGBD).

- grabber
The folder 'grabber' contains a set of header files to acquire RGB-D frames from a RGB-D sensor like Asus Xtion Pro live. The acquisition is based on OpenNI2, for what this project provides a wrapper.

- apps
Contains the source files of the above mentioned applications

-third_party
Contains third party files, employed for acquisition (OpenNI2, http://www.openni.org/), undistorting depth images (CLAMS, http://cs.stanford.edu/people/teichman/octo/clams/) and serialization (cvSerialization)


--------------- Dependencies --------------

Compulsory
OpenNI2 * Dynamic libraries must be copied into the execution path (generally the build folder with the binaries)

Optional

Third party
Clams needs to be built

---------------- Compiling ----------------


------------- Running the apps -------------
All the applications must be run in their native directory (the folder where they are created) to avoud problems linking the dynamic library libOpenNI2.so)

Syntax and description:

./OpenNIGrabber
	This application shows a dialog to choose a sensor from a list of available RGB-D cameras, and to choose the resolution (the available modes are VGA or QVGA). After this, the stream of RGB and depth images are displayed.



/*
    ROS driver to broadcast stereo images from a stereo webcam (eg. Minoru)    
    Copyright (C) 2010 Bob Mottram and Giacomo Spigler
    fuzzgun@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <sensor_msgs/Image.h>
#include <image_transport/image_transport.h>
#include <sstream>

#include <iostream>
#include <cv.h>
#include <highgui.h>
#include <stdio.h>

#include "stereocam/camera_active.h"
#include "stereocam/stereocam_params.h"
#include "libcam.h"
#include "libcam.cpp"

std::string dev0 = "/dev/video1";
std::string dev1 = "/dev/video0";

int fps = 30;
int ww = 320;
int hh = 240;
Camera *c = NULL;
Camera *c2 = NULL;
sensor_msgs::Image left;
sensor_msgs::Image right;
bool cam_active = false;
bool cam_active_request = false;

/*!
 * \brief stop the stereo camera
 * \param left_camera left camera object
 * \param right_camera right camera object
 */
void stop_cameras(
  Camera *&left_camera,
  Camera *&right_camera)
{
    if (left_camera != NULL) {
        delete left_camera;
        delete right_camera;
        left_camera = NULL;
        right_camera = NULL;
        cam_active = false;
    }
}

/*!
 * \brief start the stereo camera
 * \param left_camera left camera object
 * \param right_camera right camera object
 * \param left_img left image object
 * \param right_img right image object
 * \param left_device left camera device, eg. "/dev/video1"
 * \param right_device right camera device, eg. "/dev/video0"
 * \param width desired image width
 * \param height desired image height
 * \param fps desired frames per second
 */
void start_cameras(
  Camera *&left_camera,
  Camera *&right_camera,
  sensor_msgs::Image &left_img,
  sensor_msgs::Image &right_img,
  std::string left_device,
  std::string right_device,
  int width,
  int height,
  int fps)
{
    stop_cameras(left_camera, right_camera);

    left_camera = new Camera(left_device.c_str(), width, height, fps);
    right_camera = new Camera(right_device.c_str(), width, height, fps);

    left_img.width  = width;
    left_img.height = height;
    left_img.step = width * 3;
    left_img.encoding = "bgr8";
    left_img.set_data_size(width*height*3);

    right_img.width  = width;
    right_img.height = height;
    right_img.step = width * 3;
    right_img.encoding = "bgr8";
    right_img.set_data_size(width*height*3);

    cam_active = true;
}

/*!
 * \brief received a request to start or stop the stereo camera
 * \param req requested parameters
 * \param res returned parameters
 */
bool camera_active(
  stereocam::camera_active::Request &req,
  stereocam::camera_active::Response &res)
{
    if ((int)req.camera_active > 0) {
        ROS_INFO("Camera On");
        cam_active_request = true;
    }
    else {
        ROS_INFO("Camera Off");
        cam_active_request = false;
    }
    res.ack = 1;
    return(true);
} 

/*!
 * \brief service requests camera parameters to be changed
 * \param req requested parameters
 * \param res returned parameters
 */ 
bool request_params(
  stereocam::stereocam_params::Request &req,
  stereocam::stereocam_params::Response &res)
{
    ROS_INFO("Resolution: %dx%d", (int)req.width, (int)req.height);
    start_cameras(c,c2,left,right,req.left_device,req.right_device,req.width,req.height,req.fps);
    res.ack = 1;
    return(true);
} 

int main(int argc, char** argv)
{
  ros::init(argc, argv, "stereocam_broadcast");
  ros::NodeHandle n;

  image_transport::ImageTransport it(n);
  image_transport::Publisher left_pub = it.advertise("stereo/left/image_raw", 1);
  image_transport::Publisher right_pub = it.advertise("stereo/right/image_raw", 1);
  ros::Rate loop_rate(20);
  int count = 0;

  IplImage *l=cvCreateImage(cvSize(ww, hh), 8, 3);
  IplImage *r=cvCreateImage(cvSize(ww, hh), 8, 3);

  unsigned char *l_=(unsigned char *)l->imageData;
  unsigned char *r_=(unsigned char *)r->imageData;

  // start service which can be used to start and stop the stereo camera
  ros::ServiceServer service_active = n.advertiseService("camera_active", camera_active);

  // start service which can be used to change camera parameters
  ros::ServiceServer service_params = n.advertiseService("stereocam_params", request_params);

  ROS_INFO("Stereo camera node running");
  ROS_INFO("Waiting for subscribers...");

  while (ros::ok())
  {
    // request to turn camera on or off
    if (cam_active_request != cam_active) {
        if (cam_active_request == false) {
            stop_cameras(c,c2);
        }
        else {
            start_cameras(c,c2,left,right,dev0,dev1,ww,hh,fps);
        }
    }

    if (cam_active) {

        // Read image data
        while(c->Get()==0 || c2->Get()==0) usleep(100);

        // Convert to IplImage
        c->toIplImage(l);
        c2->toIplImage(r);

        // Convert to sensor_msgs::Image
        memcpy ((void*)(&left.data[0]), (void*)l_, ww*hh*3);
        memcpy ((void*)(&right.data[0]), (void*)r_, ww*hh*3);

        // Publish
        left_pub.publish(left);
        right_pub.publish(right);

        ROS_INFO("Stereo images published");
    }

    ros::spinOnce();
    loop_rate.sleep();
    ++count;
  }

  cvReleaseImage(&l);
  cvReleaseImage(&r);
  stop_cameras(c,c2);
}


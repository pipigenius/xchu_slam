/**
* @Program: Xchu slam
* @Description: GNSS Odometry
* @Author:  xchu
* @Create: 2020/11/26
* @Copyright: [2020] <Copyright 2022087641@qq.com>
**/


#include "xchu_slam/gps_odom.h"

int main(int argc, char **argv) {
  ros::init(argc, argv, "gps_odom_node");

  ros::NodeHandle nh;
  ros::NodeHandle node_handle("~");
  GNSSOdom gps_node(node_handle);

  ros::Rate loop_rate(100);
  while (ros::ok()) {
    ros::spinOnce();
    loop_rate.sleep();
    gps_node.run();
  }

  return 0;
}

void GNSSOdom::run() {
  if (!init_xyz) {
    ROS_WARN("PLease init lla first!!!");
    return;
  }

  if (!gpsBuf.empty() && !imuBuf.empty()) {
    // 时间戳对齐
    ros::Time gps_stamp = gpsBuf.front()->header.stamp;

    //    sensor_msgs::Imu imu_msg;
    bool imu_type = false;
    auto imu_iter = imuBuf.begin();
    for (imu_iter; imu_iter != imuBuf.end(); imu_iter++) {
      if (gps_stamp < (*imu_iter)->header.stamp) {
        break;
      }
      //      imu_msg.linear_acceleration = (*imu_iter)->linear_acceleration;
      //      imu_msg.angular_velocity = (*imu_iter)->angular_velocity;
      //      imu_msg.orientation = (*imu_iter)->orientation;
      //      imu_msg.orientation_covariance = (*imu_iter)->orientation_covariance;
      //      imu_msg.linear_acceleration_covariance = (*imu_iter)->linear_acceleration_covariance;
      //      imu_msg.angular_velocity_covariance = (*imu_iter)->angular_velocity_covariance;
      //      imu_msg.header.stamp = (*imu_iter)->header.stamp;
      imu_type = true;
    }
    mutex_lock.lock();
    imuBuf.erase(imuBuf.begin(), imu_iter);
    sensor_msgs::NavSatFixConstPtr gps_msg = gpsBuf.front();
    sensor_msgs::Imu imu_msg = *(imuBuf.front());
    gpsBuf.pop_front();
    mutex_lock.unlock();

    // 检验时间戳是否一致
    double imu_time = imu_msg.header.stamp.toSec();
    double gps_time = gps_msg->header.stamp.toSec();
    double off_time = gps_time - imu_time;
    if (off_time < 0.1 && off_time > -0.1) {
      //ROS_WARN("off set time: %f ", off_time);
    } else {
      ROS_ERROR("Time aligned failed....");
    }

    //  convert  LLA to XYZ
    Eigen::Vector3d lla = gtools.GpsMsg2Eigen(*gps_msg);
    Eigen::Vector3d ecef = gtools.LLA2ECEF(lla);
    Eigen::Vector3d enu = gtools.ECEF2ENU(ecef);
    //ROS_INFO("GPS ENU XYZ : %f, %f, %f", enu(0), enu(1), enu(2));

    // pub odom
    nav_msgs::Odometry odom_msg;
    odom_msg.header.stamp = gps_msg->header.stamp;
    odom_msg.header.frame_id = "map";
    odom_msg.child_frame_id = "gps";

    // ----------------- 1. use utm -----------------------
    //        odom_msg.pose.pose.position.x = utm_x - origin_utm_x;
    //        odom_msg.pose.pose.position.y = utm_y - origin_utm_y;
    //        odom_msg.pose.pose.position.z = msg->altitude - origin_al;

    // ----------------- 2. use enu -----------------------
    odom_msg.pose.pose.position.x = enu(0);
    odom_msg.pose.pose.position.y = enu(1);
    odom_msg.pose.pose.position.z = enu(2);
    odom_msg.pose.covariance[0] = gps_msg->position_covariance[0];
    odom_msg.pose.covariance[7] = gps_msg->position_covariance[4];
    odom_msg.pose.covariance[14] = gps_msg->position_covariance[8];

    if (imu_type) {
      odom_msg.pose.pose.orientation = imu_msg.orientation;
      odom_msg.pose.covariance[21] = imu_msg.orientation_covariance[0];
      odom_msg.pose.covariance[22] = imu_msg.orientation_covariance[1];
      odom_msg.pose.covariance[23] = imu_msg.orientation_covariance[2];
      odom_msg.pose.covariance[27] = imu_msg.orientation_covariance[3];
      odom_msg.pose.covariance[28] = imu_msg.orientation_covariance[4];
      odom_msg.pose.covariance[29] = imu_msg.orientation_covariance[5];
      odom_msg.pose.covariance[33] = imu_msg.orientation_covariance[6];
      odom_msg.pose.covariance[34] = imu_msg.orientation_covariance[7];
      odom_msg.pose.covariance[35] = imu_msg.orientation_covariance[8];

      odom_msg.twist.twist.linear = imu_msg.linear_acceleration;
      odom_msg.twist.covariance[0] = imu_msg.linear_acceleration_covariance[0];
      odom_msg.twist.covariance[7] = imu_msg.linear_acceleration_covariance[4];
      odom_msg.twist.covariance[14] = imu_msg.linear_acceleration_covariance[8];

      odom_msg.twist.twist.angular = imu_msg.angular_velocity;
      odom_msg.twist.covariance[21] = imu_msg.angular_velocity_covariance[0];
      odom_msg.twist.covariance[28] = imu_msg.angular_velocity_covariance[4];
      odom_msg.twist.covariance[35] = imu_msg.angular_velocity_covariance[8];
      imu_type = false;
    }

    gps_odom_pub_.publish(odom_msg);
  }

  /*if (!gpsBuf.empty() && !imuBuf.empty()) {
    mutex_lock.lock();

    // 时间戳对齐
    double offset_time = gpsBuf.front()->header.stamp.toSec() - imuBuf.front().header.stamp.toSec();
    std::cout << std::setprecision(10) << "offset time: " << offset_time << std::endl;
    *//*  if (std::abs(gpsBuf.front()->header.stamp.nsec - imuBuf.front().header.stamp.nsec) > 0.05) {
          ROS_ERROR("TIME STAMP UNALIGNED ERROR!!!");
          std::cout << "delta time: "
                    << std::abs(gpsBuf.front()->header.stamp.toSec() - imuBuf.front().header.stamp.toSec())
                    << std::endl;
          gpsBuf.pop();
          mutex_lock.unlock();
          continue;
        }*//*

      // time aligned
      ros::Time imu_time = imuBuf.front().header.stamp;
      sensor_msgs::Imu imu_msg = imuBuf.front();
      sensor_msgs::NavSatFixConstPtr gps_msg = gpsBuf.front();
      imuBuf.pop();
      gpsBuf.pop();
      mutex_lock.unlock();

      //  convert  LLA to XYZ
      Eigen::Vector3d lla = gtools.GpsMsg2Eigen(*gps_msg);
      Eigen::Vector3d ecef = gtools.LLA2ECEF(lla);
      Eigen::Vector3d enu = gtools.ECEF2ENU(ecef);
      gtools.gps_pos_ = enu;
      ROS_INFO("GPS ENU XYZ : %f, %f, %f", enu(0), enu(1), enu(2));
      //   std::cout << "Ins enu yaw: " << wrapToPmPi(deg2rad(msg->azimuth)) << std::endl;

      // pub odom
      nav_msgs::Odometry odom_msg;
      odom_msg.header.stamp = imu_time;
      odom_msg.header.frame_id = "map";
      odom_msg.child_frame_id = "gps_odom";

      // ----------------- 1. use utm -----------------------
      //        odom_msg.pose.pose.position.x = utm_x - origin_utm_x;
      //        odom_msg.pose.pose.position.y = utm_y - origin_utm_y;
      //        odom_msg.pose.pose.position.z = msg->altitude - origin_al;

      // ----------------- 2. use enu -----------------------
      odom_msg.pose.pose.position.x = enu(0);
      odom_msg.pose.pose.position.y = enu(1);
      odom_msg.pose.pose.position.z = enu(2);
      odom_msg.pose.covariance[0] = gps_msg->position_covariance[0];
      odom_msg.pose.covariance[7] = gps_msg->position_covariance[4];
      odom_msg.pose.covariance[14] = gps_msg->position_covariance[8];

      odom_msg.pose.pose.orientation = imu_msg.orientation;
      odom_msg.pose.covariance[21] = imu_msg.orientation_covariance[0];
      odom_msg.pose.covariance[22] = imu_msg.orientation_covariance[1];
      odom_msg.pose.covariance[23] = imu_msg.orientation_covariance[2];
      odom_msg.pose.covariance[27] = imu_msg.orientation_covariance[3];
      odom_msg.pose.covariance[28] = imu_msg.orientation_covariance[4];
      odom_msg.pose.covariance[29] = imu_msg.orientation_covariance[5];
      odom_msg.pose.covariance[33] = imu_msg.orientation_covariance[6];
      odom_msg.pose.covariance[34] = imu_msg.orientation_covariance[7];
      odom_msg.pose.covariance[35] = imu_msg.orientation_covariance[8];

      odom_msg.twist.twist.linear = imu_msg.linear_acceleration;
      odom_msg.twist.covariance[0] = imu_msg.linear_acceleration_covariance[0];
      odom_msg.twist.covariance[7] = imu_msg.linear_acceleration_covariance[4];
      odom_msg.twist.covariance[14] = imu_msg.linear_acceleration_covariance[8];

      odom_msg.twist.twist.angular = imu_msg.angular_velocity;
      odom_msg.twist.covariance[21] = imu_msg.angular_velocity_covariance[0];
      odom_msg.twist.covariance[28] = imu_msg.angular_velocity_covariance[4];
      odom_msg.twist.covariance[35] = imu_msg.angular_velocity_covariance[8];

      gpsOdomPub_.publish(odom_msg);
    }*/
//  }
}

GNSSOdom::GNSSOdom(ros::NodeHandle &node_handle) : nh_(node_handle) {
  nh_.getParam("origin_longitude", origin_longitude);
  nh_.getParam("origin_latitude", origin_latitude);
  nh_.getParam("origin_altitude", origin_altitude);
  nh_.getParam("imu_topic", imu_topic);
  nh_.getParam("gps_topic", gps_topic);
  nh_.getParam("use_localmap", use_localmap);  // 使用点云地图原点, 否则使用车辆运动的起点作为地图原点

  std::cout << "imu_topic : " << imu_topic << std::endl;
  std::cout << "gps_topic : " << gps_topic << std::endl;
  std::cout << "use_localmap : " << use_localmap << std::endl;

  std::cout << "origin_longitude : " << origin_longitude << std::endl;
  std::cout << "origin_latitude : " << origin_latitude << std::endl;
  std::cout << "origin_altitude : " << origin_altitude << std::endl;

  if (use_localmap) {
    // 使用已知的原点的话，需要设置初始LLA, 比如基于先验地图的定位
    gtools.lla_origin_ << origin_latitude, origin_longitude, origin_altitude;
  }
  ROS_INFO("Load param success!!");

  imu_sub_ = nh_.subscribe(imu_topic, 2000, &GNSSOdom::imuCB, this);  // imu topic 100hz
  gps_sub_ = nh_.subscribe(gps_topic, 100, &GNSSOdom::GNSSCB, this);  // gps topic 20hz
  gps_odom_pub_ = nh_.advertise<nav_msgs::Odometry>("/gps_odometry", 4, false);
}

void GNSSOdom::imuCB(const sensor_msgs::ImuConstPtr &msg) {
  // 是否需要调整IMU角度朝向, 一般与车头一致,有些imu需要做角度补偿
  // sensor_msgs::Imu imu_msg;
  //  imuUpsideDown(msg);  //  补偿imu yaw角
  // imu_msg = *msg;

  // 只装数据
  mutex_lock.lock();
  imuBuf.push_back(msg);
  mutex_lock.unlock();
}

void GNSSOdom::GNSSCB(const sensor_msgs::NavSatFixConstPtr &msg) {
  if (std::isnan(msg->latitude + msg->longitude + msg->altitude)) {
    ROS_ERROR("INS POS LLA NAN...");
    return;
  }
  // 设置世界坐标系原点
  if (!use_localmap && !init_xyz) {
    ROS_INFO("Init Orgin GPS LLA  %f, %f, %f", msg->latitude, msg->longitude, msg->altitude);
    gtools.lla_origin_ << msg->latitude, msg->longitude, msg->altitude;
    init_xyz = true;
  }

  // 装数据
  mutex_lock.lock();
  gpsBuf.push_back(msg);
  mutex_lock.unlock();
}

void GNSSOdom::imuUpsideDown(const sensor_msgs::Imu::Ptr &input) {
  double input_roll, input_pitch, input_yaw;

  tf::Quaternion input_orientation;
  tf::quaternionMsgToTF(input->orientation, input_orientation);
  tf::Matrix3x3(input_orientation).getRPY(input_roll, input_pitch, input_yaw);

  //  调整速度方向
  input->angular_velocity.x *= -1;
  input->angular_velocity.y *= -1;
  input->angular_velocity.z *= -1;

  input->linear_acceleration.x *= -1;
  input->linear_acceleration.y *= -1;
  input->linear_acceleration.z *= -1;

//  调整旋转方向
  input_roll *= -1;
  input_pitch *= -1;
  input_yaw *= -1;

//  input_yaw += M_PI / 2;
  input->orientation = tf::createQuaternionMsgFromRollPitchYaw(input_roll, input_pitch, input_yaw);
}

#include "ukf.h"
#include "Eigen/Dense"
#include<iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  is_initialized_=false;

  // initial state vector
  x_ = VectorXd(5);
  //x_.fill(0.0);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.75;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.65;

  //State dimension variable init
  n_x_=5;

  //Augmented state dimension
  n_aug_=7;

  Xsig_pred_=MatrixXd(n_x_,2*n_aug_+1);
  Xsig_pred_.fill(0.0);
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  lambda_=3 - n_x_;

  P_<<1.0,0,0,0,0,
      0,1.0,0,0,0,
      0,0,1.0,0,0,
      0,0,0,1.0,0,
      0,0,0,0,1.0;
  
  R_radar_ = MatrixXd(3, 3);
  R_radar_ << std_radr_*std_radr_, 0.0, 0.0,
              0.0, std_radphi_*std_radphi_, 0.0,
              0.0, 0.0,std_radrd_*std_radrd_;

  R_lidar_ = MatrixXd(2, 2);
  R_lidar_ << std_laspx_*std_laspx_,0.0,
              0.0,std_laspy_*std_laspy_;

  weights_=VectorXd(2*n_aug_+1);
      // set weights
 
 /* for (int i=0; i<2*n_aug_+1; ++i) {  // 2n+1 weights
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }*/
  weights_.fill(0.5 / (n_aug_ + lambda_));
   weights_(0) = lambda_/(lambda_+n_aug_);
 
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
    if ( !is_initialized_) {
    if (meas_package.sensor_type_ == MeasurementPackage::SensorType::RADAR) {
      double rho = meas_package.raw_measurements_[0]; // range
      double phi = meas_package.raw_measurements_[1]; // bearing
      double rho_dot = meas_package.raw_measurements_[2]; // velocity of rh
      double x = rho * cos(phi);
      double y = rho * sin(phi);
      double vx = rho_dot * cos(phi);
  	  double vy = rho_dot * sin(phi);
      double v = sqrt(vx * vx + vy * vy);
      x_ << x, y, v, 0, 0;
    } else {
      x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0, 0, 0;
    }
    time_us_ = meas_package.timestamp_ ;
    is_initialized_ = true;
    return;
  }

  double delt=(meas_package.timestamp_-time_us_)/ 1000000.0;
  time_us_=meas_package.timestamp_;
  Prediction(delt);
  
  if(meas_package.sensor_type_==MeasurementPackage::SensorType::LASER && use_laser_)
  {
    UpdateLidar(meas_package);
  }
  else if (meas_package.sensor_type_==MeasurementPackage::SensorType::RADAR && use_radar_)
  {
    UpdateRadar(meas_package);  
  }

  
}

void UKF::Prediction(double delt) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

    //Augmented state matrix
  Eigen::VectorXd x_aug_ = VectorXd(7);

  //Augmented covariance matrix
  Eigen::MatrixXd P_aug_ = MatrixXd(7,7);
  
  P_aug_.fill(0.0);
  P_aug_.topLeftCorner(5,5) = P_;
  P_aug_(5,5) = std_a_*std_a_;
  P_aug_(6,6) = std_yawdd_*std_yawdd_;

  x_aug_.head(5)=x_;
  x_aug_(5)=0.0;
  x_aug_(6)=0.0;
  //std::cout<<x_aug_<<std::endl;

  //Sigma points Generation
  MatrixXd A=P_aug_.llt().matrixL();
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  Xsig_aug.col(0)=x_aug_;
   for (int i = 0; i < n_aug_; ++i) {
    Xsig_aug.col(i+1)     = x_aug_ + sqrt(lambda_+n_aug_) * A.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug_ - sqrt(lambda_+n_aug_) * A.col(i);
  }

  //std::cout<<Xsig_aug<<std::endl<<std::endl;

  //Predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++) {
    // extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    // predicted state values
    double px_p, py_p;

    // avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + (v/yawd) * ( sin (yaw + yawd*delt) - sin(yaw));
        py_p = p_y + (v/yawd) * ( cos(yaw) - cos(yaw+yawd*delt) );
    } else {
        px_p = p_x + v*delt*cos(yaw);
        py_p = p_y + v*delt*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delt;
    double yawd_p = yawd;

    // add noise
    px_p = px_p + 0.5*nu_a*delt*delt * cos(yaw);
    py_p = py_p + 0.5*nu_a*delt*delt * sin(yaw);
    v_p = v_p + nu_a*delt;

    yaw_p = yaw_p + 0.5*nu_yawdd*delt*delt;
    yawd_p = yawd_p + nu_yawdd*delt;

    // write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }



  // predicted state mean
 x_.fill(0.0);
 //for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // iterate over sigma points
 //  x_ = x_ +  weights_(i) * Xsig_pred_.col(i);
// }
 // std::cout<<Xsig_pred_<<std::endl<<weights_<<std::endl;
  x_= Xsig_pred_ * weights_;

  // predicted state covariance matrix
 P_.fill(0.0);
  for (int i = 0; i < ((2 * n_aug_) + 1); i++) {  // iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
   while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
  }
  
  
// std::cout<<std::endl<<std::endl<<P_<<std::endl<<std::endl;


}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
  int n_z=2;
  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);

  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  //MatrixXd Zsig = Xsig_pred_.block(0, 0, n_z, 2 * n_aug_ + 1);

     // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // extract values for better readability
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);

    // measurement model
    Zsig(0,i) = p_x;   // px
    Zsig(1,i) = p_y;   // py
  }

    // mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; ++i) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

    // innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;


    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

    // add measurement noise covariance matrix

  S = S + R_lidar_;

  MatrixXd Tc = MatrixXd(n_x_, n_z);
   // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;


    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
    // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  VectorXd z = VectorXd(n_z);
  z=meas_package.raw_measurements_;

  // residual
  VectorXd z_diff = z - z_pred;


  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */
  int n_z=3;

    // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);

  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);

   // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // extract values for better readability
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                       // r
    Zsig(1,i) = atan2(p_y,p_x);                                // phi
    Zsig(2,i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   // r_dot
  }
  // mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; ++i) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }
  // innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }
  // add measurement noise covariance matrix

  S = S + R_radar_;

  MatrixXd Tc = MatrixXd(n_x_, n_z);
   // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  VectorXd z = VectorXd(n_z);
  z=meas_package.raw_measurements_;

  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

}
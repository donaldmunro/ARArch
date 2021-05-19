#ifndef _CALIBRATIONVALUES_H_
#define _CALIBRATIONVALUES_H_

#include <fstream>
#include <cmath>

#include <Eigen/Geometry>
#include <Eigen/Dense>
#include <Eigen/Eigen>

#include <opencv2/core/core.hpp>
#include <opencv2/core/eigen.hpp>

class CalibrationValues
//======================
{
public:
   /**
    * @param width The image width at which the calibration was done. Use scale() to scale to a new display width if necessary.
    * * @param height The image height at which the calibration was done. Use scale() to scale to a new display width if necessary.
    */
   CalibrationValues(cv::Mat intrinsics, int width, int height, cv::Mat distortion =cv::Mat::zeros(5,1, CV_64FC1),
                     const double rmserr =0, const double totalerr =0) :
      _intrinsics(intrinsics), _distortion(distortion), _image_width(width), _image_height(height),
      _rms_error(rmserr), _total_error(totalerr)
   //------------------------------------------------------------------------------------------------
   {
      init(false);
   }

   /**
    * @param width The image width at which the calibration was done. Use scale() to scale to a new display width if necessary.
    * * @param height The image height at which the calibration was done. Use scale() to scale to a new display width if necessary.
    */
   CalibrationValues(cv::Matx33d &intrinsicsx, int width, int height, cv::Mat distortion =cv::Mat::zeros(5,1, CV_64FC1),
                     double rmserr =0, double totalerr =0) :
         _intrinsics(intrinsicsx), _distortion(distortion), _image_width(width), _image_height(height),
         _rms_error(rmserr), _total_error(totalerr)
   //------------------------------------------------------------------------------------------------
   {
      init(false);
   }

   /**
    * @param width The image width at which the calibration was done. Use scale() to scale to a new display width if necessary.
    * * @param height The image height at which the calibration was done. Use scale() to scale to a new display width if necessary.
    */
   CalibrationValues(Eigen::Matrix3d& intrinsics, int width, int height,
                     Eigen::Matrix<double, Eigen::Dynamic, 1> *distortion =nullptr,
                     double rmserr =0, double totalerr =0) : _K(intrinsics),
                     _image_width(width), _image_height(height), _rms_error(rmserr), _total_error(totalerr)
   //---------------------------------------------------------------------------------------------
   {
      if (distortion != nullptr)
         _D = *distortion;
      init(true);
   }

   CalibrationValues() = default;

   /**
    * @param width The image width at which the calibration was done. Use scale() to scale to a new display width if necessary.
    * * @param height The image height at which the calibration was done. Use scale() to scale to a new display width if necessary.
    */
   CalibrationValues(double fx,double fy, double cx, double cy, int width, int height,
                     double k1 =0, double k2 =0, double k3 =0, double p1 =0, double p2 =0,
                     double rms_error =0, double total_error =0)
   {
      _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _distortion = cv::Mat::zeros(5, 1, CV_64FC1);
      set(fx, fy, cx, cy, k1, k2, k3, p1, p2);
   }

   CalibrationValues(CalibrationValues& other)
   {
      if (! other._intrinsics.empty())
         other._intrinsics.copyTo(_intrinsics);
      if (! other._distortion.empty())
         other._distortion.copyTo(_intrinsics);
      _rms_error = other._rms_error;
      _total_error = other._total_error;
      init(false);
   }

   CalibrationValues& operator=(const CalibrationValues& other)
   //------------------------------------------------------------------------------------------------
   {
      if (&other != this)
      {
         if (! other._intrinsics.empty())
            other._intrinsics.copyTo(_intrinsics);
         if (! other._distortion.empty())
            other._distortion.copyTo(_intrinsics);
         _rms_error = other._rms_error;
         _total_error = other._total_error;
         init(false);
      }
      return *this;
   }


   CalibrationValues& operator=(CalibrationValues&& other)
   {
      if (&other != this)
      {
         if (! other._intrinsics.empty())
         {
            other._intrinsics.copyTo(_intrinsics);
            other._intrinsics.release();
         }
         if (! other._distortion.empty())
         {
            other._distortion.copyTo(_intrinsics);
            other._distortion.release();
         }
         _rms_error = other._rms_error;
         _total_error = other._total_error;
         init(false);
      }
      return *this;
   }

   bool good() { return (! _intrinsics.empty()); }

   /** Scales to a different resolution from the one at which the calibration was done.
    **/
   void scale(int newWidth, int newHeight)
   //--------------------------------------
   {
      if (_intrinsics.empty())
         return;
      double newfx = (get_fx() / static_cast<double>(_image_width)) * newWidth;
      double newfy = (get_fy() / static_cast<double>(_image_height)) * newHeight;
      double newcx = (get_cx() / static_cast<double>(_image_width)) * newWidth;
      double newcy = (get_cy() / static_cast<double>(_image_height)) * newHeight;
      _image_width = newWidth;
      _image_height = newHeight;
      set(newfx, newfy, newcx, newcy);
   }

   void read(const cv::FileNode& fn)
   //-------------------------------------
   {
      fn["intrinsics"] >> _intrinsics;
      fn["distortion"] >> _distortion;
      _image_width = (int) fn["width"];
      _image_height = (int) fn["height"];
      _rms_error = (double) fn["rms_error"];
      _total_error = (double) fn["total_error"];
      init(false);
   }

   void write(cv::FileStorage& fs) const
   //------------------------------------------
   {
      if (_intrinsics.empty())
         return;
      fs << "CalibrationValues" << "{";
      fs << "intrinsics" << _intrinsics << "distortion" << _distortion << "width" << _image_width << "height" << _image_height
         << "rms_error" << _rms_error << "total_error" << _total_error << "}";
   }

   inline double get_fx() { return  (_intrinsics.empty()) ? std::numeric_limits<float>::quiet_NaN() : _intrinsics.at<double>(0, 0); }

   inline double get_fy() { return (_intrinsics.empty()) ? std::numeric_limits<float>::quiet_NaN() : _intrinsics.at<double>(1, 1); }

   inline double get_cx() { return  (_intrinsics.empty()) ? std::numeric_limits<float>::quiet_NaN() : _intrinsics.at<double>(2, 0); }

   inline double get_cy() { return  (_intrinsics.empty()) ? std::numeric_limits<float>::quiet_NaN() : _intrinsics.at<double>(2, 1); }

   void set_fx(double v)
   //---------------
   {
      if (_intrinsics.empty())
         _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _intrinsics.at<double>(0, 0) = v;
      init(false);
   }

   void set_fy(double v)
   //--------------
   {
      if (_intrinsics.empty())
         _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _intrinsics.at<double>(1, 1) = v;
      init(false);
   }

   void set_cx(double v)
   //---------------
   {
      if (_intrinsics.empty())
         _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _intrinsics.at<double>(2, 0) = v;
      init(false);
   }

   void set_cy(double v)
   //---------------
   {
      if (_intrinsics.empty())
         _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _intrinsics.at<double>(2, 1) = v;
      init(false);
   }

   void set(double _fx, double _fy, double _cx, double _cy, double k1 =0, double k2 =0, double k3 =0,
            double p1 =0, double p2 =0)
   //------------------------------------------------------
   {
      if (_intrinsics.empty())
         _intrinsics = cv::Mat::eye(3, 3, CV_64FC1);
      _intrinsics.at<double>(0, 0) = _fx;
      _intrinsics.at<double>(1, 1) = _fy;
      _intrinsics.at<double>(2, 0) = _cx;
      _intrinsics.at<double>(2, 1) = _cy;
      if ( (k1 != 0) || (k2 != 0) || (p1 != 0) || (p2 != 0) || (k3 != 0) )
      {
         if (_distortion.empty())
            _distortion = cv::Mat::zeros(5, 1, CV_64FC1);
         if (k1 != 0)
            _distortion.at<double>(0, 0) = k1;
         if (k2 != 0)
            _distortion.at<double>(1, 0) = k2;
         if (p1 != 0)
            _distortion.at<double>(2, 0) = p1;
         if (p2 != 0)
            _distortion.at<double>(3, 0) = p2;
         if (k3 != 0)
            _distortion.at<double>(4, 0) = k3;
      }
      init(false);
   }

   Eigen::Matrix3d K() const { return _K; }

   Eigen::Matrix3d KI() const { return _KI; }

   Eigen::Matrix3f Kf() const { return _Kf; }

   Eigen::Matrix3f KIf() const { return _KIf; }

   cv::Mat cvK() const { return _intrinsics; }

   cv::Mat cvKI() const { return _intrinsics_inverse; }

   cv::Mat cvKf() const { return _intrinsicsf; }

   cv::Mat cvKIf() const { return _intrinsics_inversef; }

   cv::Matx33d cvKx() const { return _intrinsics33d; }

   cv::Matx33d cvKIx() const { return _intrinsics_inverse33d; }

   cv::Matx33f cvKfx() const { return _intrinsics33f; }

   cv::Matx33f cvKIfx() const { return _intrinsics_inverse33f; }

private:
   cv::Mat _intrinsics,  _distortion, _intrinsics_inverse;
   cv::Mat _intrinsicsf, _intrinsics_inversef, _distortionf;
   cv::Matx33d _intrinsics33d, _intrinsics_inverse33d;
   cv::Matx33f _intrinsics33f, _intrinsics_inverse33f;
   Eigen::Matrix3d _K, _KI;
   Eigen::Matrix3f _Kf, _KIf;
   Eigen::Matrix<double, Eigen::Dynamic, 1> _D;
   Eigen::Matrix<float, Eigen::Dynamic, 1> _Df;
   int _image_width, _image_height;
   double _rms_error =0, _total_error =0;

   void init(bool isEigen)
   //---------------------------------------
   {
      if (isEigen)
      {
         cv::eigen2cv(_K, _intrinsics);
         _KI = _K.inverse();
         _Kf = _K.cast<float>();
         _KIf = _KI.cast<float>();
         if (_D.rows() > 0)
         {
            cv::eigen2cv(_D, _distortion);
            _Df = _D.cast<float>();
         }
      }
      _intrinsics.convertTo(_intrinsicsf, CV_32FC1);
      _intrinsics_inverse = _intrinsics.inv();
      _intrinsics_inverse.convertTo(_intrinsics_inversef, CV_32FC1);
      if (_distortion.empty())
         _distortion = cv::Mat::zeros(5, 1, CV_64FC1);
      else
      {
         if (_distortion.cols > _distortion.rows)
            _distortion = _distortion.t(); // Some calibration routines return 5,1 while others return 1,5
      }
      _distortion.convertTo(_distortionf, CV_32FC1);
      _intrinsics.convertTo(_intrinsics33d, CV_64FC1);
      _intrinsics_inverse33d = _intrinsics33d.inv();
      _intrinsicsf.convertTo(_intrinsics33f, CV_32FC1);
      _intrinsics_inverse33f = _intrinsics33f.inv();
      if (! isEigen)
      {
         cv::cv2eigen(_intrinsics, _K);
         _KI = _K.inverse();
         cv::cv2eigen(_distortion, _D);
         _Kf = _K.cast<float>();
         _KIf = _KI.cast<float>();
         _Df = _D.cast<float>();
      }
   }

};

#endif

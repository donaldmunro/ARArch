#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "opencv2/face.hpp"
#include <opencv2/highgui/highgui.hpp>

#include <android/log.h>
#include <mar/Structures.h>
#include <mar/util/android.hh>
#include <mar/Repository.h>

namespace toMAR
{
   namespace vision
   {
      bool YUV2RGBA(void* YUVData, void* outputJavaRGB, int w, int h, bool isRGBA, const char *logtag)
      //----------------------------------------------------------------------------------------------
      {
         cv::Mat yuv(h + h / 2, w, CV_8UC1, YUVData), rgba(h, w, CV_8UC4, outputJavaRGB);
         try //OpenCV throws exceptions.
         {
            if (isRGBA)
               cv::cvtColor(yuv, rgba, cv::COLOR_YUV2RGBA_I420);
            else
               cv::cvtColor(yuv, rgba, cv::COLOR_YUV2BGRA_I420);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                              "OpenCV exception (%s %s:%d in %s)", cverr.what(), cverr.file.c_str(),
                              cverr.line, cverr.func.c_str());
            return false;
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                  "OpenCV exception converting color data");
            return false;
         }
         return true;
      }

      bool YUV2Mono(void* YUVData, void* outputJavaGrey, int w, int h, const char *logtag)
      //---------------------------------------------------------------------------------
      {
         cv::Mat yuv(h + h / 2, w, CV_8UC1, YUVData), grey(h, w, CV_8UC1, outputJavaGrey);
         try
         {
            cv::cvtColor(yuv, grey, cv::COLOR_YUV2GRAY_I420);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "OpenCV exception cvtColor-grey (%s %s:%d in %s)", cverr.what(),
                                cverr.file.c_str(), cverr.line, cverr.func.c_str());
            return false;
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "Catchall exception converting YUV to monochrome data");
            return false;
         }
         return true;
      }

      bool NV2RGBA(void* Y, void* U, void *V, int w, int h, bool isRGBA, void* outputJavaRGB,
                   void* outputJavaGrey, const char *logtag)
      //------------------------------------------------------------------------
      {
         cv::Mat Ym(h, w, CV_8UC1, Y),  Um(h / 2, w / 2, CV_8UC2, U), Vm(h / 2, w / 2, CV_8UC2, V),
                 rgba(h, w, CV_8UC4, outputJavaRGB);
         try
         {
            if (isRGBA)
               cv::cvtColorTwoPlane(Ym, Vm, rgba, cv::COLOR_YUV2RGBA_NV21);
            else
               cv::cvtColorTwoPlane(Ym, Vm, rgba, cv::COLOR_YUV2BGRA_NV21);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "NV2RGBA: OpenCV exception cvtColorTwoPlane (%s %s:%d in %s)", cverr.what(),
                                cverr.file.c_str(), cverr.line, cverr.func.c_str());
            return false;
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "NV2RGBA: Catchall exception converting YUV to RGBA (cvtColorTwoPlane)");
            return false;
         }
         if (outputJavaGrey != nullptr)
         {
            try
            {
               cv::Mat grey(h, w, CV_8UC1, outputJavaGrey);
               cv::cvtColor(Ym, grey, cv::COLOR_YUV2GRAY_NV21);
            }
            catch (cv::Exception& cverr)
            {
               __android_log_print(ANDROID_LOG_ERROR, logtag,
                                 "NV2RGBA: OpenCV exception cvtColorTwoPlane (%s %s:%d in %s)", cverr.what(),
                                 cverr.file.c_str(), cverr.line, cverr.func.c_str());
               return false;
            }
            catch (...)
            {
               __android_log_print(ANDROID_LOG_ERROR, logtag,
                                 "NV2RGBA: Catchall exception converting YUV to RGBA (cvtColorTwoPlane)");
               return false;
            }
         }
         return true;
      }

      bool drawBB(void* img, int width, int height, double top, double left, double bottom, double right,
                  int r, int g, int b, int stroke, const char *logtag)
      //------------------------------------------------------------------------------------------------
      {
         try
         {
            cv::Mat m(height, width, CV_8UC4, img);
            cv::Point2d topLeft(top, left), bottomRight(bottom, right);
            cv::rectangle(m, topLeft, bottomRight, cv::Scalar(r, g, b), stroke);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::draw()",
                                 "vision::drawBB: OpenCV exception (%s %s:%d in %s)",
                                 cverr.what(), cverr.file.c_str(), cverr.line, cverr.func.c_str());
            return false;
         }
         catch (std::exception& e)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::draw()",
                                 "vision::drawBB: std::exception (%s)", e.what());
            return false;
         }
         catch  (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::draw()",
                                 "vision::drawBB: OpenCV Error (catchall)");
            return false;
         }
         return true;
      }

      bool overlay(void *src, int width, int height, void* dst, int w, int h,
                   const char *logtag)
      //---------------------------------------------------------------------------
      {
         try
         {
            cv::Mat m(height, width, CV_8UC4, src);
            cv::Mat overlay = cv::Mat(h, w, CV_8UC4, dst);
            cv::Rect rect = cv::Rect(0, 0, w, h); // & cv::Rect(0, 0, height, width);
            cv::Mat roi = m(rect);
////            __android_log_print(ANDROID_LOG_ERROR, logtag, "vision::overlay sizes: %dx%d %d %d | %dx%d %d %d || %d", roi.cols, roi.rows, roi.dims, roi.type(), overlay.cols, overlay.rows, overlay.dims, overlay.type(), roi.size == overlay.size);
//            cv::addWeighted(roi, 0.3, overlay, 0.7, 0, roi); // Very very slow (TODO: DIY)
            overlay.copyTo(roi);

//            cv::Size sz = cv::getContinuousSize2D(overlay, roi, overlay.elemSize());
//            int64 sz = (int64)overlay.cols * overlay.rows * overlay.elemSize();
//            bool has_int_overflow = (sz >= INT_MAX);
//            bool isContiguous = (overlay.flags & cv::Mat::CONTINUOUS_FLAG) != 0;
//            cv::Size sze = (isContiguous && !has_int_overflow)
//                   ? cv::Size(sz, 1)
//                   : cv::Size(static_cast<int>(overlay.cols * overlay.elemSize()), overlay.rows);
//            const uchar* sptr = overlay.data;
//            uchar* dptr = roi.data;
//            for (; sze.height--; sptr += overlay.step, dptr += roi.step)
//                memcpy(dptr, sptr, static_cast<size_t>(sze.width));
            return true;
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "vision::overlay: OpenCV exception creating image overlay (%s %s:%d in %s)",
                                cverr.what(), cverr.file.c_str(), cverr.line, cverr.func.c_str());
         }
         catch (std::exception& e)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "vision::overlay: OpenCV exception creating image overlay (%s)", e.what());
         }
         catch  (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                "vision::overlay: OpenCV catchall exception creating image overlay");
         }
         return false;
      }

      const char* FACE_DETECTOR_CASCADE_ASSET = "faces/haarcascade_frontalface_default.xml";
      const char* FACE_DETECTOR_CASCADE_LOCAL = "/sdcard/Documents/haarcascade_frontalface_default.xml";
      static cv::CascadeClassifier* cascade = nullptr;

      bool init_faces(void* params)
      //----------------------------
      {
         if (cascade == nullptr)
         {
            Repository *repository = Repository::instance();
            if (! copy_asset(repository->pAssetManager, FACE_DETECTOR_CASCADE_ASSET,
                             FACE_DETECTOR_CASCADE_LOCAL))
               return false;
            cascade = new cv::CascadeClassifier;
            if (! cascade->load(FACE_DETECTOR_CASCADE_LOCAL))
            {
               delete cascade;
               cascade = nullptr;
               return false;
            }
         }
         return true;
      }

      bool find_face(void *src, int width, int height, int minArea, DetectRect<int>& faceBB,
                     const char *logtag)
      //---------------------------------------------------------------------------------------------
      {
         faceBB = DetectRect<int>();
         if (cascade == nullptr) return false;
         cv::Mat rgba(height, width, CV_8UC4, src), gray;
         std::vector<cv::Rect> faces;
         try
         {
            cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY); //TODO: Support BGRA
            cv::equalizeHist(gray, gray);
            cascade->detectMultiScale(gray, faces, 1.4, 3,
                                      cv::CASCADE_SCALE_IMAGE + cv::CASCADE_FIND_BIGGEST_OBJECT);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                 "vision::faces: OpenCV exception (%s %s:%d in %s)", cverr.what(),
                                 cverr.file.c_str(), cverr.line, cverr.func.c_str());
            return false;
         }
         catch (std::exception& e)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                                 "vision::faces: std::exception %s", e.what());
            return false;
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, logtag,
                  "vision::faces: Catchall exception.");
            return false;
         }
         int maxArea = (minArea <= 0) ? 0 : minArea;
         cv::Rect roi;
         for (cv::Rect cr : faces)
         {
            if (cr.area() >= maxArea)
               roi = cr;
         }
         if (roi.area() > 0)
            faceBB = DetectRect<int>(roi.y, roi.x, roi.y + roi.height, roi.x + roi.width);
         else
            return false;
         return true;
      }

      void dump(unsigned long cid, uint64_t seqno, const char* nid, int w, int h,
                    void *framedata)
      //------------------------------------------------------------------------------------
      {
         try
         {
            cv::Mat m(h, w, CV_8UC4, framedata), mm;
            char nname[512];
            cv::cvtColor(m, mm, cv::COLOR_RGBA2BGR);
            sprintf(nname, "/sdcard/Pictures/%lu-%08lu-%s.png", cid, seqno, nid);
            cv::imwrite(nname, mm);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, "toMAR::vision::dump",
                                "OpenCV exception (%s %s:%d in %s)", cverr.what(),
                                cverr.file.c_str(), cverr.line, cverr.func.c_str());
         }
         catch (std::exception& e)
         {
            __android_log_print(ANDROID_LOG_ERROR, "toMAR::vision::dump",
                                "std::exception %s", e.what());
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, "toMAR::vision::dump",
                                "Catchall exception.");
         }
      }
   }
}

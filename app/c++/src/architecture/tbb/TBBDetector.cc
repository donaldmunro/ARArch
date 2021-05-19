#include <fstream>
#include <time.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <mar/util/util.hh>

#include "mar/architecture/tbb/TBBDetector.h"
#ifdef HAS_APRILTAGS
#include "apriltags/apriltag.h"
#include "apriltags/tag36h11.h"
//#include "apriltags/tag36h10.h"
//#include "apriltags/tag36artoolkit.h"
#include "apriltags/tag25h9.h"
//#include "apriltags/tag25h7.h"
#endif
#include "mar/util/android.hh"
#include "mar/util/util.hh"
#include "mar/util/cv.h"

namespace toMAR
{
   std::atomic_bool TBBSimulationDetector::isDetecting{false};

    uint64_t TBBSimulationDetector::operator()(uint64_t seqno)
    //------------------------------------------------
    {
       Repository *repository = Repository::instance();
       std::shared_ptr<FrameInfo> frame;
       if (repository->get_frame(camera1Id, seqno, frame))
       {
          spin(isDetecting, frame.get(), detectRate);
          frame->isDetecting.store(false);
          repository->delete_frame(camera1Id,  seqno);
       }
       return seqno;
    }

   bool TBBSimulationDetector::is_detecting() { return isDetecting.load(); }

#ifdef HAS_APRILTAGS
   tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> AprilTagTBBDetector::isDetecting;

   AprilTagTBBDetector::AprilTagTBBDetector(unsigned long camera1, unsigned long camera2) :
                                            Detector(),
                                            camera1Id(camera1), camera2Id(camera2),
                                            repository(Repository::instance())
   //-----------------------------------------------------------------------------------
   {
      isDetecting[camera1] = new std::atomic_bool(false);
      init();
   }

   bool AprilTagTBBDetector::init()
   //-----------------------------
   {
      detector = apriltag_detector_create();
      if (detector == nullptr) return false;
      apriltag_detector_add_family(detector, tag36h11_create());
      apriltag_detector_add_family(detector, tag25h9_create());
      detector->quad_decimate = 1.0;
      detector->quad_sigma = 0.0;
      detector->nthreads = 2;
      detector->debug = 0;
      detector->refine_edges = 1;
      return true;
   }

   AprilTagTBBDetector::~AprilTagTBBDetector()
   //-----------------------------------------
   {
      if (detector != nullptr)
      {
         apriltag_detector_destroy(detector);
         detector = nullptr;
      }
      auto it = isDetecting.find(camera1Id);
      if (it != isDetecting.end())
      {
         if (it->second)
            delete it->second;
      }
   }


   uint64_t AprilTagTBBDetector::operator()(uint64_t seqno)
   //----------------------------------------------------------
   {
       std::shared_ptr<FrameInfo> frame;
      if (! repository->get_frame(camera1Id, seqno, frame))
         return seqno;
//      __android_log_print(ANDROID_LOG_INFO, "AprilTagTBBDetector::operator()",
//                          "Detector frame %lu", frame->seqno);

      isDetecting[camera1Id]->store(true);

//      bool currently_detecting = false;  //not necessary - parallelism is set to 1
//      if (! isDetecting.compare_exchange_strong(currently_detecting, true)) return seqno;
      void* env;
      cv::Mat gray;
      unsigned char* framedata = frame->getColorData(env);
      try
      {
         cv::Mat rgba(frame->height, frame->width, CV_8UC4, framedata);
         cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);
      }
      catch (cv::Exception& cverr)
      {
         __android_log_print(ANDROID_LOG_ERROR, "AprilTagTBBDetector::operator()",
                             "OpenCV exception (%s %s:%d in %s)", cverr.what(), cverr.file.c_str(),
                             cverr.line, cverr.func.c_str());
      }
      catch (...)
      {
         __android_log_print(ANDROID_LOG_ERROR, "AprilTagTBBDetector::operator()",
                             "OpenCV exception converting color data 1");
      }
      frame->releaseColorData(env, framedata);
      image_u8_t im = { .width = gray.cols, .height = gray.rows, .stride = gray.cols,.buf = gray.data};
      zarray_t*  detections = apriltag_detector_detect(detector, &im);

      if (detections != nullptr)
      {
         tbb::concurrent_hash_map<uint64_t, std::vector<DetectedBoundingBox*>>& targets = repository->aprilTags;
         std::vector<DetectedBoundingBox*> L;
         for (int i = 0; i < zarray_size(detections); i++)
         {
            apriltag_detection_t *det;
            zarray_get(detections, i, &det);
//            cv::Rect2d rect(det->p[0][0], det->p[0][1], det->p[3][0] - det->p[0][0],
//                            det->p[3][1] - det->p[0][1]);
            L.push_back(new DetectedBoundingBox(seqno, camera1Id, det->p[0][0], det->p[0][1],
                                                 det->p[2][0], det->p[2][1]));
            apriltag_detection_destroy(det);
         }
         zarray_destroy(detections);
         targets.insert(std::make_pair(seqno, L));
         repository->recentAprilTags.push(seqno);
      }
      // As we don't have stereo calibration (see Kaliber) we can't do much with stereo info,
      // so just do detection on stereo camera for benchmark without using the detections.
      unsigned long camera2;
      uint64_t seqno2;
      std::shared_ptr<FrameInfo> frame2;
      if ( (repository->stereo_twin(camera1Id, seqno, camera2, seqno2)) &&
           (repository->get_frame(camera2, seqno2, frame2)) )
      {
         framedata = frame2->getColorData(env);
         try
         {
            cv::Mat rgba2(frame2->height, frame2->width, CV_8UC4, framedata);
            cv::cvtColor(rgba2, gray, cv::COLOR_RGBA2GRAY);
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, "AprilTagTBBDetector::operator()",
                                "OpenCV exception (%s %s:%d in %s)", cverr.what(), cverr.file.c_str(),
                                cverr.line, cverr.func.c_str());
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, "AprilTagTBBDetector::operator()",
                                "OpenCV exception converting color data 2");
         }
         frame2->releaseColorData(env, framedata);
         image_u8_t im2 = { .width = gray.cols, .height = gray.rows, .stride = gray.cols,.buf = gray.data};
         zarray_t*  detections2 = apriltag_detector_detect(detector, &im2);
//         __android_log_print(ANDROID_LOG_INFO, "AprilTagTBBDetector::operator()", "Stereo frame detections for %lu: %d", seqno2, zarray_size(detections2));
         if (detections2 != nullptr)
         {
            for (int i = 0; i < zarray_size(detections2); i++)
            {
               apriltag_detection_t *det;
               zarray_get(detections2, i, &det);
               apriltag_detection_destroy(det);
            }
            zarray_destroy(detections2);
         }
      }

      if (frame)
      {
         frame->isDetecting.store(false);
         repository->delete_frame(camera1Id,  seqno);
      }
      if ( (camera2Id != std::numeric_limits<unsigned long>::max()) &&
           (repository->get_frame(camera2Id, seqno, frame2)) )
      {
         frame->isDetecting.store(false);
         repository->delete_frame(camera2Id,  seqno);
      }

      isDetecting[camera1Id]->store(false);
      return seqno;
   }

/*   static void decorate(cv::Mat* frame, const zarray_t *detections)
   //---------------------------------------------------------------
   {
      for (int i = 0; i < zarray_size(detections); i++)
      {
         apriltag_detection_t *det;
         zarray_get(detections, i, &det);
         line(*frame, cv::Point(det->p[0][0], det->p[0][1]),
              cv::Point(det->p[1][0], det->p[1][1]),
              cv::Scalar(0, 0xff, 0), 2);
         line(*frame, cv::Point(det->p[0][0], det->p[0][1]),
              cv::Point(det->p[3][0], det->p[3][1]),
              cv::Scalar(0, 0, 0xff), 2);
         line(*frame, cv::Point(det->p[1][0], det->p[1][1]),
              cv::Point(det->p[2][0], det->p[2][1]),
              cv::Scalar(0xff, 0, 0), 2);
         line(*frame, cv::Point(det->p[2][0], det->p[2][1]),
              cv::Point(det->p[3][0], det->p[3][1]),
              cv::Scalar(0xff, 0, 0), 2);

         std::stringstream ss;
         ss << det->id;
         cv::String text = ss.str();
         int fontface = cv::FONT_HERSHEY_SCRIPT_SIMPLEX;
         double fontscale = 1.0;
         int baseline;
         cv::Size textsize = cv::getTextSize(text, fontface, fontscale, 2, &baseline);
         cv::putText(frame, text, cv::Point(det->c[0] - textsize.width / 2, det->c[1] + textsize.height / 2),
                     fontface, fontscale, cv::Scalar(0xff, 0x99, 0), 2);
         //   apriltag_detection_destroy(det);
      }
   }

   static void apriltag_detections_destroy(zarray_t *detections)
   //----------------------------------------------------------
   {
      for (int i = 0; i < zarray_size(detections); i++)
      {
         apriltag_detection_t *det;
         zarray_get(detections, i, &det);

         apriltag_detection_destroy(det);
      }

      zarray_destroy(detections);
   }
   */
#endif

#ifdef HAS_FACE_DETECTION
   tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> FaceTBBDetector::isDetecting;
   tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> FaceOverlayTBBDetector::isDetecting;

   uint64_t FaceTBBDetector::operator()(uint64_t seqno)
   //-------------------------------------------------
   {
      std::shared_ptr<FrameInfo> frame;
      if (! repository->get_frame(cameraId, seqno, frame))
         return seqno;
      isDetecting[cameraId]->store(true);
      void* env;
      unsigned char* framedata = frame->getColorData(env);
      DetectRect<int> faceBB;
      if (toMAR::vision::find_face(framedata, frame->width, frame->height, 90000, faceBB,
                               "FaceTBBDetector::()"))
      {
         cv::Rect roi(faceBB.top, faceBB.left, faceBB.width(), faceBB.height());
         repository->faceDetections.insert(std::make_pair(seqno,
                                           new DetectedBoundingBox(seqno, cameraId, roi.y, roi.x,
                                           roi.y + roi.height, roi.x + roi.width)));
         repository->recentFaceDetections.push(seqno);
         // __android_log_print(ANDROID_LOG_INFO, "FaceTBBDetector::operator()",
         //                     "Found face %d,%d %dx%d",
         //                     roi.x, roi.y, roi.x + roi.width, roi.y + roi.height);
      }
      frame->releaseColorData(env, framedata);
      isDetecting[cameraId]->store(false);
      frame->isDetecting.store(false);
      repository->delete_frame(cameraId, seqno);
      last_detection = toMAR::util::now_monotonic();
      return seqno;
   }

   FaceTBBDetector::~FaceTBBDetector()
   //---------------------------------
   {
      auto it = isDetecting.find(cameraId);
      if (it != isDetecting.end())
      {
         if (it->second)
            delete it->second;
      }
   }

   uint64_t FaceOverlayTBBDetector::operator()(uint64_t seqno)
   //-----------------------------------------------------------
   {
      std::shared_ptr<FrameInfo> frame;
      if (! repository->get_frame(cameraId, seqno, frame))
         return seqno;
      const int64_t now = toMAR::util::now_monotonic();
      isDetecting[cameraId]->store(true); // required for router
      void* env;
      unsigned char* framedata = frame->getColorData(env);
      DetectRect<int> faceBB;
      if (toMAR::vision::find_face(framedata, frame->width, frame->height, 20000, faceBB,
                                   "FaceOverlayTBBDetector::()"))
      {
         cv::Rect roi(faceBB.top, faceBB.left, faceBB.width(), faceBB.height());
         // __android_log_print(ANDROID_LOG_INFO, "FaceOverlayTBBDetector::operator()",
         //                     "Found face %d,%d %dx%d",
         //                     roi.x, roi.y, roi.x + roi.width, roi.y + roi.height);
         tbb::concurrent_hash_map<uint64_t, DetectedROI*>& overlays = repository->faceOverlays;
         std::vector<uint64_t> dels;
         int count = 0;
         uint64_t delSeq = repository->currentRenderOverlay.load() - 1;
         for (auto it=overlays.begin(); it!=overlays.end(); ++it)
         {
            if (count++ > 5) break;
            DetectedROI *lastOverlay = it->second;
            if (lastOverlay->inUse) continue;
            uint64_t seq = it->first;
            if (seq < delSeq)
            {
               if (lastOverlay != nullptr)
               {
                  dels.push_back(seq);
                  if (lastOverlay->image != nullptr)
                  {
                     cv::Mat* pm = static_cast<cv::Mat*>(lastOverlay->image);
                     delete pm;
                  }
                  lastOverlay->image = nullptr;
                  delete lastOverlay;
//                     it = overlays.erase(it);
                  // __android_log_print(ANDROID_LOG_INFO, "FaceOverlayTBBDetector::()", "Deleted face overlay %lu", seq);
               }
            }
         }
         for (uint64_t sn : dels) overlays.erase(sn);
         try
         {
            cv::Mat rgba(frame->height, frame->width, CV_8UC4, framedata);
            cv::Mat* m = new cv::Mat(roi.height, roi.width, CV_8UC4);  //new cv::Mat(rgba, roi);
            cv::Rect croppedRoi = roi & cv::Rect(0, 0, rgba.cols, rgba.rows); // https://answers.opencv.org/question/70953/roi-out-of-bounds-issue/
            rgba(croppedRoi).copyTo(*m);
            overlays.insert(std::make_pair(seqno, new DetectedROI(seqno,  cameraId,
                                           static_cast<void *>(m),
                                           roi.y, roi.x, roi.y + roi.height, roi.x + roi.width)));
            repository->lastFaceOverlay.store(seqno);
            // __android_log_print(ANDROID_LOG_INFO, "FaceOverlayTBBDetector::()",
            //                     "Pushed overlay %lu %lu %dx%d %dx%d %p %p", cameraId, seqno, roi.width, roi.height, m->cols, m->rows,
            //                     static_cast<void *>(rgba.data), static_cast<void *>(m->data));
         }
         catch (cv::Exception& cverr)
         {
            __android_log_print(ANDROID_LOG_ERROR, "FaceOverlayTBBDetector::operator()",
                                "OpenCV exception (%s %s:%d in %s)", cverr.what(),
                                cverr.file.c_str(), cverr.line, cverr.func.c_str());
         }
         catch (...)
         {
            __android_log_print(ANDROID_LOG_ERROR, "FaceOverlayTBBDetector::operator()",
                                "Catchall exception.");
         }
      }
      frame->releaseColorData(env, framedata);
      isDetecting[cameraId]->store(false);
      frame->isDetecting.store(false);
      repository->delete_frame(cameraId, seqno);
      return seqno;
   }

   FaceOverlayTBBDetector::~FaceOverlayTBBDetector()
   //-----------------------------------------------
   {
      auto it = isDetecting.find(cameraId);
      if (it != isDetecting.end())
      {
         if (it->second)
            delete it->second;
      }
   }
#endif

   uint64_t TBBNullDetector::operator()(uint64_t seqno)
   //-------------------------------------------------
   {
      Repository* repository = Repository::instance();
      std::shared_ptr<FrameInfo> frame;
      if (repository->get_frame(cameraId, seqno, frame))
      {
         frame->isDetecting.store(false);
         repository->delete_frame(cameraId, seqno);
      }
      return seqno;
   }


}

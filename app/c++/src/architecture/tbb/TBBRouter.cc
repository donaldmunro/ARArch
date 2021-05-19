#include <tuple>

#include <android/log.h>

#include "mar/architecture/tbb/TBBRouter.h"

namespace toMAR
{
   void TBBRouter::operator()(const uintptr_t in,
                   tbb::flow::multifunction_node<uintptr_t,
                   RouterOutputTuple>::output_ports_type& out)
   //----------------------------------------------------------------------------------------
   {
//      if (! repository->initialised.load()) return;
      CameraFrame* cameraFrame = (CameraFrame*) in;
      // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Router %lu %lu %lu %lu %d",
      //                     cameraFrame->cameras[0].cameraId, cameraFrame->cameras[0].seqno_1,
      //                     cameraFrame->cameras[1].cameraId, cameraFrame->cameras[1].seqno_1,
      //                     cameraFrame->count);
      if (cameraFrame->count == 0)
         return;

////      tbb::flow::interface11::internal::function_output<unsigned long> xxx = std::get<0>(out);
      for (CameraFrameData frameData : cameraFrame->cameras)
      {
         unsigned long camera1 = frameData.cameraId;
         if (camera1 == std::numeric_limits<unsigned long>::max())
            break;
         uint64_t seqno = frameData.seqno_1, seqno2 = frameData.seqno_2;
         unsigned long camera2 = frameData.camera2Id;
//         unsigned long twinCamera;
////         if (camera2 == std::numeric_limits<unsigned long>::max())
//         bool isStereo = repository->stereo_twin(camera1, seqno, twinCamera, seqno2);
            // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "routing %lu %lu %lu", camera1, camera2, seqno);
         for (std::pair<unsigned long, uint64_t> pp :
             { std::make_pair(camera1, seqno), std::make_pair(camera2, seqno2) } )
         {
            unsigned long cameraId = pp.first;
            if (cameraId == std::numeric_limits<unsigned long>::max()) break;
            uint64_t seq = pp.second;
            if (seq == 0) continue;
            std::shared_ptr<FrameInfo> frame;
            if (! repository->get_frame(cameraId, seq, frame))
               continue;
            Detector* detectorPtr;
            Tracker* trackerPtr;
            Renderer* rendererPtr;
            int portStart;
            bool isDelete;
            auto it = map.find(cameraId);
            if (it != map.end())
            {
               TBBRouterParameters& params = it->second;
               portStart = params.portStart;
               detectorPtr = params.detector;
               trackerPtr = params.tracker;
               rendererPtr = params.renderer;
               isDelete = params.isDelete;
            }
            else
               continue;

            bool isDetectRoute = false, isTrackRoute = false;
            switch (portStart)
            {
               case 0:
                  if ( (detectorPtr != nullptr) && (! detectorPtr->is_detecting()) )
                  {
                     isDetectRoute = std::get<0>(out).try_put(seq);
                     if (isDetectRoute)
                     {
                        isDelete = false;
                        frame->isDetecting.store(true);
                     // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to detector %d", cameraId, seqno, isDetectRoute);
                     }
                  }
                  if ( (trackerPtr != nullptr) && (! isDetectRoute) && (! trackerPtr->is_tracking()) )
                  {
                     isTrackRoute = std::get<1>(out).try_put(seq);
                     if (isTrackRoute)
                     {
                        isDelete = false;
                        frame->isTracking.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to tracker %d", cameraId, seqno, isTrackRoute);
                     }
                  }
                  if (rendererPtr != nullptr)
                  {
                     if (! std::get<2>(out).try_put(seq))
                        repository->delete_frame(cameraId, seq);
                      else
                     {
                         frame->isRendering.store(true);
//                         __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to renderer", cameraId, seqno);
                     }
                  }
                  else if (isDelete)
                  {
                     repository->delete_frame(cameraId, seq);
                     // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Deleted %lu %lu", cameraId, seqno);
                  }
                  break;
               case 3:
                  if ( (detectorPtr != nullptr) && (! detectorPtr->is_detecting()) )
                  {
                     isDetectRoute = std::get<3>(out).try_put(seq);
                     if (isDetectRoute)
                     {
                        isDelete = false;
                        frame->isDetecting.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to detector %d", cameraId, seqno, isDetectRoute);
                     }
                  }
                  if ( (trackerPtr != nullptr) && (! isDetectRoute) && (! trackerPtr->is_tracking()) )
                  {
                     isTrackRoute = std::get<4>(out).try_put(seq);
                     if (isTrackRoute)
                     {
                        isDelete = false;
                        frame->isTracking.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to tracker %d", cameraId, seqno, isTrackRoute);
                     }
                  }
                  if (rendererPtr != nullptr)
                  {
                     if (! std::get<5>(out).try_put(seq))
                        repository->delete_frame(cameraId, seq);
                      else
                     {
                        frame->isRendering.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to renderer", cameraId, seqno);
                     }
                  }
                  else if (isDelete) repository->delete_frame(cameraId, seq);
                  break;
               case 6:
                  if ( (detectorPtr != nullptr) && (! detectorPtr->is_detecting()) )
                  {
                     isDetectRoute = std::get<6>(out).try_put(seq);
                     if (isDetectRoute)
                     {
                        isDelete = false;
                        frame->isDetecting.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to detector %d", cameraId, seqno, isDetectRoute);
                     }
                  }
                  if ( (trackerPtr != nullptr) && (! isDetectRoute) && (! trackerPtr->is_tracking()) )
                  {
                     isTrackRoute = std::get<7>(out).try_put(seq);
                     if (isTrackRoute)
                     {
                        isDelete = false;
                        frame->isTracking.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to tracker %d", cameraId, seqno, isTrackRoute);
                     }
                  }
                  if (rendererPtr != nullptr)
                  {
                     if (! std::get<8>(out).try_put(seq))
                        repository->delete_frame(cameraId, seq);
                      else
                     {
                        frame->isRendering.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to renderer", cameraId, seqno);
                     }
                  }
                  else if (isDelete) repository->delete_frame(cameraId, seq);
                  break;
               case 9:
                  if ( (detectorPtr != nullptr) && (! detectorPtr->is_detecting()) )
                  {
                     isDetectRoute =  std::get<9>(out).try_put(seq);
                     if (isDetectRoute)
                     {
                        isDelete = false;
                        frame->isDetecting.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to detector %d", cameraId, seqno, isDetectRoute);
                     }
                  }
                  if ( (trackerPtr != nullptr) && (! isDetectRoute) && (! trackerPtr->is_tracking()) )
                  {
                     isTrackRoute = std::get<10>(out).try_put(seq);
                     if (isTrackRoute)
                     {
                        isDelete = false;
                        frame->isTracking.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to tracker %d", cameraId, seqno, isTrackRoute);
                     }
                  }
                  if (rendererPtr != nullptr)
                  {
                     if (! std::get<11>(out).try_put(seq))
                        repository->delete_frame(cameraId, seq);
                      else
                     {
                        frame->isRendering.store(true);
                        // __android_log_print(ANDROID_LOG_INFO, "TBBRouter::operator()", "Routed %lu %lu to renderer", cameraId, seqno);
                     }
                  }
                  else
                     if (isDelete) repository->delete_frame(cameraId, seq);
                  break;
               default:
                  __android_log_print(ANDROID_LOG_ERROR, "TBBRouter::operator()",
                                      "Invalid port offset in router (%d)", portStart);
                  break;

            }
         }
      }
   }
}

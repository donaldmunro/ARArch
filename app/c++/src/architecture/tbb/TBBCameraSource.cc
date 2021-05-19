#include <stack>
#include <memory>
#include <limits>
#include <tuple>

#include <android/log.h>

#include "mar/architecture/tbb/TBBCameraSource.h"

//#define LAST_2_BEST_TIMESTAMP
//#define LAST_2_TIMESTAMP
#define BOTTOM2

namespace toMAR
{
   bool TBBMonoCameraSourceNode::init()
   //----------------------------------
   {
      camera_interface = repository->hardware_camera_interface_ptr(cameraId);
      if (! camera_interface)
      {
         __android_log_print(ANDROID_LOG_ERROR, "TBBMonoCameraSourceNode::init",
                             "Camera %lu not found", cameraId);
         return false;
      }
      camera_interface->clear();
      return true;
   }

   bool TBBMonoCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //---------------------------------------------------------
   {
      std::shared_ptr<FrameInfo> frame;
      CameraFrame* cameraFrame = new CameraFrame;
      camera_interface->dequeue_blocked(frame);
      if (frame) //(camera_interface->dequeue(frame))
      {
         uint64_t seq = repository->new_frame(cameraId, frame); // + offset;
         cameraFrame->set(0, cameraId, seq);
//         __android_log_print(ANDROID_LOG_INFO, "TBBMonoCameraSourceNode::operator()", "Camera %lu: Enqueued %lu", cameraId, seq);
      }
      pcameraFrame = (uintptr_t) cameraFrame;
//      if (! camera_interface->dequeue_timed(frame, 1000000000LL))
//      {
//         __android_log_print(ANDROID_LOG_WARN, "TBBMonoCameraSourceNode::()", "Timed out waiting for camera %lu frame.", cameraId);
//         seqno = 0;
//         return (! repository->must_terminate);
//      }
//      __android_log_print(ANDROID_LOG_INFO, "TBBMonoCameraSourceNode::()", "Enqueued seq %lu for camera %lu",
//                          seqno, cameraId);
      return (! repository->must_terminate);
   }

   bool TBBDualMonoCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //-------------------------------------------------------------------
   {
      CameraFrame* cameraFrame = new CameraFrame;
      std::shared_ptr<FrameInfo> frame1, frame2;
      size_t i = 0;
      if (camera1Interface->dequeue(frame1))
      {
         uint64_t seq1 = repository->new_frame(camera1Id, frame1);
         cameraFrame->set(i++, camera1Id, seq1);
      }
      if (camera2Interface->dequeue(frame2))
      {
         uint64_t seq2 = repository->new_frame(camera2Id, frame2);
         cameraFrame->set(i, camera2Id, seq2);
      }
      pcameraFrame = (uintptr_t) cameraFrame;
      return (! repository->must_terminate);
   }

   TBBDualMonoCameraSourceNode::TBBDualMonoCameraSourceNode(unsigned long camera1, unsigned camera2):
         camera1Id(camera1), camera2Id(camera2), repository(Repository::instance())
   //-----------------------------------------------------------------------------------------------
   {
      camera1Interface = repository->hardware_camera_interface_ptr(camera1);
      camera2Interface = repository->hardware_camera_interface_ptr(camera2);
   }

   bool TBBStereoCameraSourceNode::init()
   //-------------------------------------------------------------------------------------------------
   {
      std::shared_ptr<Camera> interface;
      if ( (! repository->hardware_camera_interface(camera1Id, interface)) || (! interface) )
      {
         __android_log_print(ANDROID_LOG_ERROR, "TBBStereoCameraSourceNode::init", "Camera %lu not found",
                             camera1Id);
         is_ok = false;
         return false;
      }
      else
      {
         camera0_interface = interface;
         is_ok = true;
      }
      if ( (repository->hardware_camera_interface(camera2Id, interface)) && (interface) )
         camera1_interface = interface;
      else
      {
         __android_log_print(ANDROID_LOG_ERROR, "TBBJavaCamera::TBBJavaCamera", "Camera %lu not found",
                             camera2Id);
         camera1_interface.reset();
      }
      camera0_interface->clear();
      if (camera1_interface)
         camera1_interface->clear();
      return true;
   }

#ifdef LAST_2_BEST_TIMESTAMP
   bool TBBStereoCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //----------------------------------------------------------
   {
      CameraFrame* cameraFrame = new CameraFrame;
      cameraFrame->seq = 0;
      if ( (camera0_interface) && (camera1_interface) &&
           (camera0_interface->queue_size() > 0) && (camera1_interface->queue_size() > 0) )
      {
         std::stack<std::shared_ptr<FrameInfo>> stack0, stack1;
         long items0 = camera0_interface->drain(stack0);
         long items1 = camera1_interface->drain(stack1);
         if ( (items0 <= 0) && (items1 <= 0) )
            return (! repository->must_terminate);
         std::shared_ptr<FrameInfo> data0, data1;
         std::shared_ptr<FrameInfo> d0[2];
         std::shared_ptr<FrameInfo> d1[2];
         d0[0] = stack0.top(); stack0.pop();
         d0[1] = (stack0.size() >= 1) ? stack0.top() : std::shared_ptr<FrameInfo>();
         if (d0[1]) stack0.pop();
         d1[0] = stack1.top(); stack1.pop();
         d1[1] = (stack1.size() >= 1) ? stack1.top() : std::shared_ptr<FrameInfo>();
         if (d1[1]) stack1.pop();
         int64_t bestdiff = std::numeric_limits<int64_t>::max();
         int ii = -1, jj =-1;
         for (int i=0; i<2; i++)
         {
            for (int j=0; j<2; j++)
            {
               if ( (! d0[i]) || (! d1[j]) ) continue;
               int64_t diff = std::abs(d0[i]->timestamp - d1[j]->timestamp);
               if (diff < bestdiff)
               {
                  data0 = d0[i];
                  data1 = d1[j];
                  bestdiff = diff;
                  ii = i;
                  jj = j;
               }
            }
         }
         for (int i=0; i<2; i++)
         {
            for (int j=0; j<2; j++)
            {
               if (i != ii) d0[i].reset();
               if (j != jj) d1[j].reset();
            }
         }

         while (stack0.size() > 0) { stack0.top().reset(); stack0.pop(); }
         while (stack1.size() > 0) { stack1.top().reset(); stack1.pop(); }

         cameraFrame->cameraId = camera0_interface->camera_id();
         cameraFrame->camera2Id = camera1_interface->camera_id();
         cameraFrame->seq = repository->new_stereo_frame(camera0_interface->camera_id(), data0,
                                                         camera1_interface->camera_id(), data1);
      }
      pcameraFrame = (uintptr_t) cameraFrame;
      return (! repository->must_terminate);
   }
#endif
#ifdef LAST_2_TIMESTAMP
   bool TBBStereoCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //---------------------------------------------------------------------------
   {
      CameraFrame* cameraFrame = new CameraFrame;
      cameraFrame->seq = 0;
      if ( (camera0_interface) && (camera1_interface) &&
           (camera0_interface->queue_size() > 0) && (camera1_interface->queue_size() > 0) )
      {
         std::stack<std::shared_ptr<FrameInfo>> stack0, stack1;
         long c0 = camera0_interface->drain(stack0);
         long c1 = camera1_interface->drain(stack1);
         if ((c0 == 0) && (c1 == 0) )
            return (! repository->must_terminate);

         std::shared_ptr<FrameInfo> frame0, frame1;
         if (c0 > 0)
         {
            frame0 = stack0.top();
            stack0.pop();
            while (! stack0.empty()) { stack0.top().reset(); stack0.pop(); }
         }
         if (c1 > 0)
         {
            frame1 = stack1.top();
            stack1.pop();
            while (! stack1.empty()) { stack1.top().reset(); stack1.pop(); }
         }

         cameraFrame->cameraId = camera0_interface->camera_id();
         cameraFrame->camera2Id = camera1_interface->camera_id();
         cameraFrame->seq = repository->new_stereo_frame(camera0_interface->camera_id(), frame0,
                                                         camera1_interface->camera_id(), frame1);
      }
      pcameraFrame = (uintptr_t) cameraFrame;
      return (! repository->must_terminate);
   }
#endif
#ifdef BOTTOM2
   bool TBBStereoCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //---------------------------------------------------------------------------
   {
      CameraFrame* cameraFrame = new CameraFrame;
      const long qs1 = camera0_interface->queue_size();
      const long qs2 = camera1_interface->queue_size();
      std::shared_ptr<FrameInfo> frame1, frame2;
      if ( (qs1 > 0) && (qs2 > 0) )
      {
         if (! camera0_interface->dequeue(frame1))
            return (! repository->must_terminate);
         if (! camera1_interface->dequeue(frame2))
            return (! repository->must_terminate);
         unsigned long camera1 = camera0_interface->camera_id();
         unsigned long camera2 = camera1_interface->camera_id();
         uint64_t seq = repository->new_stereo_frame(camera1, frame1, camera2, frame2);
         cameraFrame->set_stereo(0, camera1, camera2, seq);
      }
      pcameraFrame = (uintptr_t) cameraFrame;
      return (! repository->must_terminate);
   }
#endif

   bool TBBVoidCameraSourceNode::operator()(uintptr_t& pcameraFrame)
   //---------------------------------------------------------------
   {
      CameraFrame* cameraFrame = new CameraFrame;
      cameraFrame->set(0, 0, 0);
      pcameraFrame = (uintptr_t) cameraFrame;
      return true;
   }
}

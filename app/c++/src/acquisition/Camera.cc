#include <stdlib.h>
#include <time.h>

#include "mar/acquisition/Camera.h"
#include "mar/Repository.h"

namespace toMAR
{
   Camera::Camera(std::string &cameraId, size_t queueSize, bool isRearFacing, int width,
                  int height) : previewWidth(width), previewHeight(height),
                                isRearFacing(isRearFacing), maxQueueSize(queueSize)
#ifdef LOCK_FREE_QUEUE
                                ,queue(queueSize*sizeof(FrameInfo) + sizeof(FrameInfo))
#endif
   {
      repository = Repository::instance();
      id = camera_ID(cameraId);
#ifndef LOCK_FREE_QUEUE
      queue.set_capacity(queueSize);
#endif
   }

#ifdef LOCK_FREE_QUEUE
   bool Camera::enqueue(FrameInfo* data)
   //------------------------------------
   {
      std::shared_ptr<FrameInfo> sp(data);
      if (! queue.try_enqueue(sp))
      {
         __android_log_print(ANDROID_LOG_WARN, "Camera::enqueue", "Queue full");
         std::shared_ptr<FrameInfo> old_data;
         for (int i=0; i<2; i++)
         {
            if (queue.try_dequeue(old_data))
            {
               uint64_t seq = old_data->seqno;
               old_data.reset();
               repository->delete_frame(seq);
            }
            if (queue.try_enqueue(sp))
               return true;
         }
      }
      else
         return true;
      return false;
   }

   void Camera::clear()
   //------------------
   {
      std::shared_ptr<FrameInfo> old_data;
      while (queue.try_dequeue(old_data)) old_data.reset();
   }

   long Camera::drain(std::stack<std::shared_ptr<FrameInfo>>& stack)
   //-----------------------------------------------------------------
   {
      size_t n = queue.size_approx();
      long no = 0;
      for (size_t i=0; i<n; ++i)
      {
         std::shared_ptr<FrameInfo> data;
         bool isMore = queue.try_dequeue(data);
         if (isMore)
            stack.push(data);
         else
            return no;
         no++;
      }
      return no;
   }

   bool Camera::dequeue(std::shared_ptr<FrameInfo> &data) { return queue.try_dequeue(data); }

   long Camera::queue_size() { return queue.size_approx(); }
#else
   bool Camera::enqueue(FrameInfo* data)
   //------------------------------------
   {
      std::shared_ptr<FrameInfo> sp(data);
      if (! queue.try_push(sp))
      {
         __android_log_print(ANDROID_LOG_WARN, "Camera::enqueue", "Queue full");
         std::shared_ptr<FrameInfo> old_data;
         for (int i=0; i<2; i++)
         {
            if (queue.try_pop(old_data))
            {
               uint64_t seq = old_data->seqno;
               old_data.reset();
               repository->delete_frame(id, seq);
            }
            if (queue.try_push(sp))
               return true;
         }
      }
      else
         return true;
      return false;
   }

   void Camera::clear()
   //------------------
   {
      std::shared_ptr<FrameInfo> old_data;
      auto n = queue.size();
      for (auto i=0; i<n; ++i)
      {
         bool isMore = queue.try_pop(old_data);
         if (! isMore)
            break;
         if (old_data)
            old_data.reset();
      }
   }

   long Camera::drain(std::stack<std::shared_ptr<FrameInfo>>& stack)
   //-----------------------------------------------------------------
   {
      auto n = queue.size();
      long no = 0;
      for (auto i=0; i<n; ++i)
      {
         std::shared_ptr<FrameInfo> data;
         bool isMore = queue.try_pop(data);
         if (isMore)
            stack.push(data);
         else
            return no;
         no++;
      }
      return no;
   }

   bool Camera::dequeue(std::shared_ptr<FrameInfo> &data) { return queue.try_pop(data); }

   bool Camera::dequeue_timed(std::shared_ptr<FrameInfo> &data, uint64_t timeout, long sleeptime)
   //---------------------------------------------------------------------------
   {
      struct timespec tt;
      tt.tv_sec = tt.tv_nsec = 0;
      clock_gettime(CLOCK_MONOTONIC, &tt);
      uint64_t then = static_cast<uint64_t>(tt.tv_sec) * 1000000000LL + tt.tv_nsec;
      while (! queue.try_pop(data))
      {
         tt.tv_sec = tt.tv_nsec = 0;
         clock_gettime(CLOCK_MONOTONIC, &tt);
         uint64_t now = static_cast<uint64_t>(tt.tv_sec) * 1000000000LL + tt.tv_nsec;
         uint64_t elapsed = now - then;
         if (elapsed > timeout)
            return false;
         tt.tv_sec = 0; tt.tv_nsec = sleeptime;
         nanosleep(&tt, nullptr);
      }
      return true;
   }

   void Camera::dequeue_blocked(std::shared_ptr<FrameInfo> &data) { queue.pop(data); }

   long Camera::queue_size() { return queue.size(); }

   unsigned long Camera::camera_ID(std::string cameraID)
   //---------------------------------------------------
   {
      char *endptr;
      unsigned long id = strtoul(cameraID.c_str(), &endptr, 10);
      if (*endptr != 0)
         id = std::hash<std::string>{}(cameraID);
      return id;
   }
#endif
};



#ifndef __MAR_CAMERA__
#define __MAR_CAMERA__
//#define LOCK_FREE_QUEUE

#include <memory>
#include <stack>

#ifdef LOCK_FREE_QUEUE
#include "lockfree/concurrentqueue.h"
#else
#include "tbb/concurrent_queue.h"
#endif

#include "mar/jniint.h"
#include "mar/acquisition/FrameInfo.h"

namespace toMAR
{
   class Camera
   //==========
   {
      public:
         Camera(std::string& id, size_t queueSize, bool isRearFacing =true,
               int width =-1, int height =-1);

         unsigned long camera_id() { return id; }

         void camera_id(std::string camera) { id = camera_ID(camera); }

         void camera_id(unsigned long camera) { id = camera; }

         bool is_rear_facing() { return  isRearFacing; }

         void preview_size(int width, int height) { previewWidth = width; previewHeight = height; }

         void get_preview_size(int& width, int& height) { width = previewWidth; height = previewHeight; }

         bool enqueue(FrameInfo* data);

         bool dequeue(std::shared_ptr<FrameInfo>& data);

         void dequeue_blocked(std::shared_ptr<FrameInfo>& data);

         bool dequeue_timed(std::shared_ptr<FrameInfo> &data, uint64_t timeout, long sleeptime=10000000LL);

         long queue_capacity() { return queue.capacity(); }

         long queue_size();

         long drain(std::stack<std::shared_ptr<FrameInfo>> &);

         void clear();

         static unsigned long camera_ID(std::string cameraID);

   private:
      Repository* repository;
      unsigned long id;
      int previewWidth, previewHeight;
      bool isRearFacing;
      size_t maxQueueSize;
#ifdef LOCK_FREE_QUEUE
      moodycamel::ConcurrentQueue<std::shared_ptr<FrameInfo>> queue;
#else
      tbb::concurrent_bounded_queue<std::shared_ptr<FrameInfo>> queue;
#endif

      friend class Repository;
   };
};
#endif

#include "mar/architecture/tbb/TBBTracker.h"
#include "mar/Repository.h"
namespace toMAR
{
   TBBTimeTestTracker::TBBTimeTestTracker(unsigned long camera, double rate) :
      Tracker(), cameraId(camera), trackRate(rate), repository(Repository::instance())
   {}

   uint64_t TBBTimeTestTracker::operator()(uint64_t seqno)
   //--------------------------------
   {
      std::shared_ptr<FrameInfo> frame;
      if (repository->get_frame(cameraId, seqno, frame))
      {
         spin(isTracking, frame.get(), trackRate);
         frame->isTracking.store(false);
         repository->delete_frame(cameraId, seqno);
      }
      return seqno;
   }

   uint64_t TBBNullTracker::operator()(uint64_t seqno)
   //------------------------------------------------
   {
      Repository* repository = Repository::instance();
      std::shared_ptr<FrameInfo> frame;
      if (repository->get_frame(cameraId, seqno, frame))
      {
         frame->isTracking.store(false);
         repository->delete_frame(cameraId,  seqno);
      }
      return seqno;
   }
}

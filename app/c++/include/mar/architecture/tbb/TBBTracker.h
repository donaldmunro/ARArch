#ifndef _TBBTRACKER_H
#define _TBBTRACKER_H

#include <cstdio>

#include <tbb/flow_graph.h>

#include "mar/architecture/tbb/TBBTimedTest.hh"

namespace toMAR
{
   class Repository;

   class Tracker
   //===========
   {
   public:
      virtual uint64_t operator()(uint64_t seqno) =0;
      virtual bool is_tracking() =0;
      virtual ~Tracker() {}
   };

   class TBBTimeTestTracker : public TBBTimedTest, public Tracker
   //============================================================
   {
   public:
      explicit TBBTimeTestTracker(unsigned long camera, double rate);

      uint64_t operator()(uint64_t seqno) override;
      bool is_tracking() override { return isTracking.load(); }

   private:
      unsigned long cameraId;
      double trackRate;
      Repository* repository;
      std::atomic_bool isTracking{false};
   };

   class TBBNullTracker : public Tracker
   //===================================
   {
   public:
      explicit TBBNullTracker(const unsigned long camera,  bool mustDelete=false) : Tracker(),
         cameraId(camera)
      {}

      uint64_t operator()(uint64_t seqno) override;
      bool is_tracking() override { return false; }
      const unsigned long cameraId;
   };

}
#endif

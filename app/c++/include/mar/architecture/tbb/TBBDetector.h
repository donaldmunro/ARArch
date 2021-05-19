#ifndef _TBBDETECTOR_H
#define _TBBDETECTOR_H

#include <cstdio>

#include <tbb/flow_graph.h>
#ifdef HAS_APRILTAGS
#include <apriltags/apriltag.h>
#endif
#ifdef HAS_FACE_DETECTION
#include "opencv2/face.hpp"
#endif
#include "mar/Repository.h"
#include "mar/architecture/tbb/TBBTimedTest.hh"
#include "mar/RunningStatistics.hh"

namespace toMAR
{
   class Detector
   //============
   {
   public:
      virtual uint64_t operator()(uint64_t seqno) =0;
      virtual bool is_detecting() =0;

      virtual ~Detector() {}
   };

   class TBBSimulationDetector : public TBBTimedTest, public Detector
   //===============================================================
   {
   public:
      TBBSimulationDetector(std::string camera1, double rate) :
         Detector(), camera1Id(Camera::camera_ID(camera1)), detectRate(rate)
      {}
      TBBSimulationDetector(unsigned long camera1, double rate) :
            camera1Id(camera1), detectRate(rate){}

      uint64_t operator()(uint64_t seqno) override;

      bool is_detecting() override;

      private:
         unsigned long camera1Id;
         double detectRate;
         static std::atomic_bool isDetecting;
   };

#ifdef HAS_APRILTAGS
   class AprilTagTBBDetector : public Detector
   //=========================================
   {
   public:
      explicit AprilTagTBBDetector(unsigned long camera1,
                                   unsigned long camera2 = std::numeric_limits<unsigned long>::max());

      uint64_t operator()(uint64_t seqno) override;

      bool is_detecting() override { return isDetecting[camera1Id]->load(); }

      ~AprilTagTBBDetector();
   private:
      bool init();

      const unsigned long camera1Id, camera2Id;
      apriltag_detector_t *detector;
      Repository* repository;
      static tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> isDetecting;
   };
#endif

#ifdef HAS_FACE_DETECTION
   class FaceTBBDetector : public Detector
   //=========================================
   {
   public:
      FaceTBBDetector(unsigned long camera) : Detector(), cameraId(camera),
         repository(Repository::instance()), last_detection(toMAR::util::now_monotonic())
      {
         isDetecting[cameraId] = new std::atomic_bool(false);
      }

      bool is_detecting() override { return isDetecting[cameraId]->load(); }

      uint64_t operator()(uint64_t seqno) override;

      bool good() { return isGood; }

      ~FaceTBBDetector();

   private:
      const unsigned long cameraId;
      Repository* repository;
      bool isGood{false};
      int64_t last_detection =0;

      static tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> isDetecting;
   };

   class FaceOverlayTBBDetector : public Detector
   //=========================================
   {
   public:
      explicit FaceOverlayTBBDetector(unsigned long camera) : Detector(),
            cameraId(camera), repository(Repository::instance())
      {
         isDetecting[cameraId] = new std::atomic_bool(false);
      }

      ~FaceOverlayTBBDetector();

      bool is_detecting() override { return isDetecting[cameraId]->load(); }

      uint64_t operator()(uint64_t seqno) override;

      bool good() { return isGood; }

//      ~FaceTBBDetector();

   private:
      const unsigned long cameraId;
      Repository* repository;
      bool isGood{false};

      static tbb::concurrent_unordered_map<unsigned long, std::atomic_bool*> isDetecting;
   };
#endif

   class TBBNullDetector : public Detector
   //===============================================================
   {
   public:
      TBBNullDetector(unsigned long camera) : Detector(), cameraId(camera) {}

      uint64_t operator()(uint64_t seqno) override;

      bool is_detecting() override { return false; }

   private:
      const unsigned long cameraId;
   };
}
#endif

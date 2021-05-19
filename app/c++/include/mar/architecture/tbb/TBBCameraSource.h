#ifndef TBB_JAVA_CAMERA_INTERFACE_
#define TBB_JAVA_CAMERA_INTERFACE_
#include <memory>

#include "tbb/concurrent_vector.h"

#include "mar/architecture/tbb/TBBCameraSource.h"
#include "mar/Repository.h"
#include "mar/acquisition/Camera.h"
#include "mar/util/RingBuffer.hh"

namespace toMAR
{

   class TBBMonoCameraSourceNode
   //========================================================
   {
   public:
      explicit TBBMonoCameraSourceNode(unsigned long camera) :
            cameraId(camera), repository(Repository::instance()), is_ok(init())
      {}

      bool good() { return is_ok; }
      bool operator()(uintptr_t& pcameraFrame); // const;

   private:
      const unsigned long cameraId;
      Camera* camera_interface;
      Repository* repository;
      bool is_ok;

      bool init();
   };

   class TBBDualMonoCameraSourceNode
   //===============================
   {
   public:
      TBBDualMonoCameraSourceNode(unsigned long camera1, unsigned camera2);

      bool good() { return is_ok; }
      bool operator()(uintptr_t& pcameraFrame); // const;

   private:
      const unsigned long camera1Id, camera2Id;
      Camera *camera1Interface, *camera2Interface;
      Repository* repository;
      bool is_ok;
   };

   class TBBStereoCameraSourceNode
   //==========================================================
   {
   public:
      TBBStereoCameraSourceNode(std::string camera1, std::string camera2) :
         camera1Id(Camera::camera_ID(camera1)), camera2Id(Camera::camera_ID(camera2)),
            repository(Repository::instance()), is_ok(init())
      {}

      TBBStereoCameraSourceNode(unsigned long camera1, unsigned long camera2):
            camera1Id(camera1), camera2Id(camera2), repository(Repository::instance()),
            is_ok(init())
      {}

      bool good() { return is_ok; }
      bool operator()(uintptr_t& pcameraFrame); // const;

   private:
      unsigned long camera1Id, camera2Id;
      std::shared_ptr<Camera> camera0_interface, camera1_interface;
      Repository* repository;
      bool is_ok = false;
      static std::atomic_uint64_t next_seqno;

      bool init();
   };

   class TBBVoidCameraSourceNode
   //========================================================
   {
   public:
      TBBVoidCameraSourceNode() = default;
      bool good() { return true; }
      bool operator()(uintptr_t& pcameraFrame);
   };
};
#endif

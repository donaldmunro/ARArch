#ifndef _MAR_REPOSITORY_H
#define _MAR_REPOSITORY_H

#include <cstdio>
#include <string>
#include <utility>
#include <map>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <stdexcept>
#include <filesystem>
namespace filesystem = std::filesystem;

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <jni.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_priority_queue.h>
#include <mar/util/util.hh>

#include "tbb/mutex.h"
#include "tbb/spin_mutex.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_unordered_map.h"

#include "mar/acquisition/Camera.h"
#include "mar/acquisition/FrameInfo.h"
#include "RunningStatistics.hh"
#include "mar/Structures.h"

namespace toMAR
{
   class Repository
   //==============
   {
   public:
      static Repository* instance();

      Repository(Repository const&) = delete;
      Repository(Repository&&) = delete;
      Repository& operator=(Repository const&) = delete;
      Repository& operator=(Repository &&) = delete;

      void set_native_window(ANativeWindow* nwin) {  }
      bool add_camera(std::string camera_id, int qsize, bool isRearFacing);
      bool hardware_camera_interface(unsigned long camera, std::shared_ptr<Camera>& sptr);
      Camera* hardware_camera_interface_ptr(const unsigned long camera);
      std::vector<std::shared_ptr<Camera>> rear_cameras();
      std::vector<std::shared_ptr<Camera>> front_cameras();
      std::vector<std::shared_ptr<Camera>> all_cameras();
      unsigned long no_cameras() { return cameras.size(); }
      uint64_t new_frame(const unsigned long camera, std::shared_ptr<FrameInfo>& frame);
      uint64_t new_stereo_frame(const unsigned long camera1, std::shared_ptr<FrameInfo>& frame1,
                                const unsigned long camera2, std::shared_ptr<FrameInfo>& frame2);
      void stereo_pair(const unsigned long camera1, const uint64_t seq1,
                       const unsigned long camera2, const uint64_t seq2);
      bool delete_stereo_pair(const unsigned long camera1, const uint64_t seq1);
      bool stereo_twin(const unsigned long camera, const uint64_t seq,
                       unsigned long& twinCamera, uint64_t& twinSeq);
      bool get_frame(const unsigned long camera, uint64_t seqno, std::shared_ptr<FrameInfo>& frame);
      bool has_frame(uint64_t seqno);
      bool has_frame(const unsigned long camera, uint64_t seqno);
      void delete_all_frames(uint64_t seqno);
      void delete_frame(const unsigned long camera, const uint64_t seqno);
//      bool is_calibrating() { return isCalibrating; }

      std::atomic_bool must_terminate{false};
//      bool set_is_rendering(unsigned long cameraId, bool setTo);
//      tbb::concurrent_unordered_map<unsigned long, std::unique_ptr<std::atomic_bool>> rendererBusyFlags;
      std::atomic_bool is_rendering{false};
      tbb::concurrent_unordered_map<unsigned long, std::atomic_uint64_t*> seqNumbers;
      tbb::concurrent_hash_map<size_t , std::pair<unsigned long, uint64_t>> stereoFrames;
      AAssetManager* pAssetManager = nullptr;
      tbb::concurrent_unordered_map<unsigned long, RunningStatistics<uint64_t, long double>*>
         detectorStatistics;
      RunningStatistics<uint64_t, long double>* detectionStats(const unsigned long camera,
                                                               bool isCreate=true);
      void clear_detector_stats(const unsigned long camera);
      tbb::concurrent_unordered_map<unsigned long, RunningStatistics<uint64_t, long double>*>
            renderStatistics;
      RunningStatistics<uint64_t, long double>* rendererStats(const unsigned long camera,
                                                              bool isCreate=true);
      void clear_render_stats(const unsigned long camera);

      tbb::concurrent_hash_map<uint64_t, std::vector<DetectedBoundingBox*>> aprilTags;
      tbb::concurrent_priority_queue<uint64_t, UInt64DescComparator> recentAprilTags;
//      tbb::concurrent_priority_queue<std::vector<DetectedBoundingBox*>> recentAprilTags;
      tbb::concurrent_hash_map<uint64_t, DetectedBoundingBox*> faceDetections;
      tbb::concurrent_priority_queue<uint64_t, UInt64DescComparator> recentFaceDetections;
      tbb::concurrent_hash_map<uint64_t, DetectedROI*> faceOverlays;
//      tbb::concurrent_priority_queue<uint64_t, UInt64DescComparator> recentFaceOverlays;
      std::atomic_uint64_t lastFaceOverlay{0};
      std::atomic_uint64_t currentRenderOverlay{1};

      void javaVM(JavaVM *pvm);
      JavaVM* javaVM();

      void clear_queued(unsigned long camera);

      std::atomic_bool initialised{false};

      uint64_t next_seqno(const unsigned long camera);

   private:
      tbb::concurrent_unordered_map<unsigned long, std::shared_ptr<Camera>> cameras;
      tbb::concurrent_hash_map<unsigned long,
                      tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*> camera_frames;
      JavaVM* vm;


      Repository() = default;
      ~Repository();
   };
};

#endif //MAR_REPOSITORY_H

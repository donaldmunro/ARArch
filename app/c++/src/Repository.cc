#include <vector>
#include <cassert>
#include "tbb/mutex.h"
#include "mar/Repository.h"

namespace toMAR
{
   Camera* Repository::hardware_camera_interface_ptr(unsigned long camera)
   //---------------------------------------------------------------------
   {
      auto it = cameras.find(camera);
      if (it != cameras.end())
      {
         std::shared_ptr<Camera> sptr = it->second;
         if (sptr)
         {
            Camera* ptr = (*it).second.get();
            return ptr;
         }
      }
      return nullptr;
   }

   bool Repository::hardware_camera_interface(unsigned long camera, std::shared_ptr<Camera>& sptr)
   //--------------------------------------------------------------------------------------------------------
   {
      auto it = cameras.find(camera);
      if (it != cameras.end())
         sptr = (*it).second;
      else
         return false;
      return (sptr.get() != nullptr);
   }

   std::vector<std::shared_ptr<Camera>> Repository::rear_cameras()
   //--------------------------------------------------------------
   {
      std::vector<std::shared_ptr<Camera>> ret;
      for (auto it=cameras.begin(); it != cameras.end(); ++it)
      {
         std::shared_ptr<Camera> sp = it->second;
         if (sp->is_rear_facing())
            ret.push_back(sp);
      }
      return ret;
   }

   std::vector<std::shared_ptr<Camera>> Repository::front_cameras()
   //--------------------------------------------------------------
   {
      std::vector<std::shared_ptr<Camera>> ret;
      for (auto it=cameras.begin(); it != cameras.end(); ++it)
      {
         std::shared_ptr<Camera> sp = it->second;
         if (! sp->is_rear_facing())
            ret.push_back(sp);
      }
      return ret;
   }

   std::vector<std::shared_ptr<Camera>> Repository::all_cameras()
   //-----------------------------------------------------------
   {
      std::vector<std::shared_ptr<Camera>> ret;
      for (auto it=cameras.begin(); it != cameras.end(); ++it)
         ret.push_back(it->second);
      return ret;
   };

   bool Repository::add_camera(std::string camera, int qsize, bool isRearFacing)
   //----------------------------------------------------------
   {
      unsigned long id = Camera::camera_ID(camera);
      if (id == std::numeric_limits<unsigned long>::max())
         return false;

      auto it = cameras.find(id);
      if (it == cameras.end())
      {
         std::shared_ptr<Camera> sptr = std::make_shared<Camera>(camera, qsize, isRearFacing);
         cameras.insert(std::make_pair(id, sptr));
         assert(cameras.count(id) == 1);
         clear_detector_stats(id);
         clear_render_stats(id);
         return (cameras.count(id) == 1);
      }
      else
      {
         std::shared_ptr<Camera> interface = (*it).second;
         if (interface->camera_id() != id)
         {
            interface->camera_id(id);
            cameras[id] = interface;
            clear_detector_stats(id);
            clear_render_stats(id);
            return true;
         }
      }
      return false;
   }

   uint64_t Repository::new_frame(unsigned long camera, std::shared_ptr<FrameInfo>& frame)
   //------------------------------------------------------------------------------------------
   {
      uint64_t seqno = next_seqno(camera);
//      __android_log_print(ANDROID_LOG_WARN, "Repository::new_frame()", "Seq: %lu Camera %lu", seqno, camera);
      frame->seqno = seqno;
      tbb::concurrent_hash_map<unsigned long,
               tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor it;
      if (! camera_frames.find(it, camera))
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames =
               new tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>;
         camera_frames.insert(std::make_pair(camera, frames));
         frames->insert(std::make_pair(seqno, frame));
      }
      else
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = it->second;
         if (frames)
            frames->insert(std::make_pair(seqno, frame));
      }
      return seqno;
   }

   uint64_t Repository::new_stereo_frame(const unsigned long camera1, std::shared_ptr<FrameInfo> &frame1,
                                         const unsigned long camera2, std::shared_ptr<FrameInfo> &frame2)
   //------------------------------------------------------------------------------------------------
   {
      uint64_t seqno = next_seqno(camera1);
      tbb::concurrent_hash_map<unsigned long,
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor it;
      if (! camera_frames.find(it, camera1))
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames =
               new tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>;
         camera_frames.insert(std::make_pair(camera1, frames));
         frames->insert(std::make_pair(seqno, frame1));
      }
      else
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = it->second;
         if (frames)
            frames->insert(std::make_pair(seqno, frame1));
      }
      if (! camera_frames.find(it, camera2))
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames =
               new tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>;
         camera_frames.insert(std::make_pair(camera2, frames));
         frames->insert(std::make_pair(seqno, frame2));
      }
      else
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = it->second;
         if (frames)
            frames->insert(std::make_pair(seqno, frame2));
      }
      return seqno;
   }

   bool Repository::get_frame(const unsigned long camera, uint64_t seqno,
                              std::shared_ptr<FrameInfo>& frame)
   //-------------------------------------------------------------------------------------------
   {
      tbb::concurrent_hash_map<unsigned long,
               tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor itc;
      if (! camera_frames.find(itc, camera))
         return false;
      const tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
      tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
      if ( (frames == nullptr) || (! frames->find(itf, seqno)) )
         return false;
      frame = itf->second;
      if ( (frame) &&  (frame->rgbaLen > 0) )
         return true;
      else
      {
         if (frame)
         {
            uint64_t seq = frame->seqno;
            frame.reset();
            delete_frame(camera, seq);
         }
      }
      return false;
   }

   bool Repository::has_frame(uint64_t seqno)
   //----------------------------------------
   {
      for (auto it = cameras.cbegin(); it != cameras.cend(); ++it)
      {
         const unsigned long camera = it->first;
         tbb::concurrent_hash_map<unsigned long,
                  tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::const_accessor itc;
         if (camera_frames.find(itc, camera))
         {
            const tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
//            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
            if ( (frames) && (frames->count(seqno) > 0) )  //(frames->find(itf, seqno)) )
               return true;
         }
      }
      return false;
   }

   bool Repository::has_frame(const unsigned long camera, uint64_t seqno)
   //--------------------------------------------------------------------
   {
      tbb::concurrent_hash_map<unsigned long,
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::const_accessor itc;
      if (camera_frames.find(itc, camera))
      {
         const tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
//            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
         if ( (frames) && (frames->count(seqno) > 0) )  //(frames->find(itf, seqno)) )
            return true;
      }
      return false;
   }

   void Repository::delete_all_frames(uint64_t seqno)
   //-------------------------------------------
   {
      for (auto it = cameras.cbegin(); it != cameras.cend(); ++it)
      {
         const unsigned long camera = it->first;
         tbb::concurrent_hash_map<unsigned long,
                  tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor itc;
         if (camera_frames.find(itc, camera))
         {
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
            if ( (frames) && (frames->find(itf, seqno)) )
            {
               std::shared_ptr<FrameInfo> frame = itf->second;
               if ( (! frame->isDetecting.load()) && (! frame->isTracking.load()) )
                  frames->erase(itf);
//               if (frames->empty())
//               {
//                  camera_frames.erase(itc);
//                  delete frames;
//               }
            }
         }
      }
   }

   void Repository::delete_frame(const unsigned long camera, const uint64_t seqno)
   //-------------------------------------------
   {
      tbb::concurrent_hash_map<unsigned long,
               tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor itc;
      bool isFound = false;
      if (camera_frames.find(itc, camera))
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
         if ( (frames) && (frames->find(itf, seqno)) )
         {
            std::shared_ptr<FrameInfo> frame = itf->second;
            if ( (! frame->isDetecting.load()) && (! frame->isTracking.load()) &&
                 (! frame->isRendering.load()))
            {
               isFound = true;
               frames->erase(itf);
            }
//            if (frames->empty())
//            {
//               camera_frames.erase(itc);
//               delete frames;
//            }
         }
      }
      if (! isFound) return;
      unsigned long camera2;
      uint64_t seqno2;
      if (stereo_twin(camera, seqno, camera2, seqno2))
      {
         if (camera_frames.find(itc, camera2))
         {
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>::accessor itf;
            if ( (frames) && (frames->find(itf, seqno2)) )
            {
               std::shared_ptr<FrameInfo> frame = itf->second;
               if ( (! frame->isDetecting.load()) && (! frame->isTracking.load()) )
                  frames->erase(itf);
            }
         }
         delete_stereo_pair(camera, seqno);
      }
   }

   void Repository::clear_queued(unsigned long camera)
   //-------------------------------------------------
   {
      tbb::concurrent_hash_map<unsigned long,
            tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>*>::accessor itc;
      if (camera_frames.find(itc, camera))
      {
         tbb::concurrent_hash_map<uint64_t, std::shared_ptr<FrameInfo>>* frames = itc->second;
         frames->clear();
      }
   }

   Repository *Repository::instance()
   //--------------------------------
   {
      static Repository* the_instance = nullptr;
      if (the_instance == nullptr)
         the_instance = new Repository;
//      __android_log_print(ANDROID_LOG_WARN, "Repository::instance()", "Repository::instance address: %p", the_instance);
      return the_instance;
   }

   Repository::~Repository()
   //----------------------
   {
      for (auto it = cameras.cbegin(); it != cameras.cend(); ++it)
      {
         const unsigned long camera = it->first;
         clear_queued(camera);
      }
   }

   RunningStatistics<uint64_t, long double>* Repository::detectionStats(const unsigned long camera,
                                                                        bool isCreate)
   //----------------------------------------------------------------------------------------------
   {
      auto it = detectorStatistics.find(camera);
      if (it == detectorStatistics.end())
      {
         if (isCreate)
         {
            RunningStatistics<uint64_t, long double>* stats = new RunningStatistics<uint64_t, long double>;
            detectorStatistics.insert(std::make_pair(camera, stats));
            return stats;
         }
         else
            return nullptr;
      }
      else
         return  it->second;
   }

   void Repository::clear_detector_stats(const unsigned long camera)
   //---------------------------------------------------------------
   {
      auto it = detectorStatistics.find(camera);
      if ( (it != detectorStatistics.end()) && (it->second) )
         it->second->clear();
   }

   RunningStatistics<uint64_t, long double>* Repository::rendererStats(const unsigned long camera,
                                                                       bool isCreate)
   //---------------------------------------------------------------------------------------------
   {
      auto it = renderStatistics.find(camera);
      if (it == renderStatistics.end())
      {
         if (isCreate)
         {
            RunningStatistics<uint64_t, long double>* stats = new RunningStatistics<uint64_t, long double>;
            renderStatistics.insert(std::make_pair(camera, stats));
            return  stats;
         }
         else
            return nullptr;
      }
      else
         return  it->second;
   }

   void Repository::clear_render_stats(const unsigned long camera)
   //-------------------------------------------------------------
   {
      auto it = renderStatistics.find(camera);
      if ( (it != renderStatistics.end()) && (it->second) )
         it->second->clear();
   }

   void Repository::javaVM(JavaVM *pvm) { vm = pvm; }

   JavaVM *Repository::javaVM() { return vm; }

//   bool Repository::set_is_rendering(unsigned long cameraId, bool setTo)
//   //-------------------------------------------------------------------
//   {
//      auto it = rendererBusyFlags.find(cameraId);
//      if (it == rendererBusyFlags.end())
//      {
////         rendererBusyFlags[cameraId] = std::make_unique<std::atomic_bool>(setTo);
//         rendererBusyFlags.insert(std::make_pair(cameraId, std::make_unique<std::atomic_bool>(setTo)));
//         __android_log_print(ANDROID_LOG_INFO, "Repository::set_is_rendering", "Set camera %lu rendering flag %d", cameraId, setTo);
//         return setTo;
//      }
//      else
//      {
//         std::atomic_bool* pB = it->second.get();
//         if (setTo == false)
//         {
//            pB->store(false);
//            return true;
//         }
//         else
//         {
//            bool rendering = false;
//            bool b = pB->compare_exchange_strong(rendering, true);
//            __android_log_print(ANDROID_LOG_INFO, "Repository::set_is_rendering", "Reset camera %lu rendering flag %d %d", cameraId, setTo, b);
//            return b;
////            return (pB->compare_exchange_strong(rendering, true));
//         }
//      }
//   }

   uint64_t Repository::next_seqno(const unsigned long camera)
   //----------------------------------------------------------
   {
      auto it = seqNumbers.find(camera);
      if (it == seqNumbers.end())
      {
         seqNumbers[camera] = new std::atomic_uint64_t(0);
         return 0;
      }
      else
      {
         std::atomic_uint64_t *seq = it->second;
         if (seq)
            return seq->fetch_add(1);
         else
            __android_log_print(ANDROID_LOG_WARN, "Repository::next_seqno()", "No sequence for %lu", camera);
      }
      return 0;
   }

   void Repository::stereo_pair(const unsigned long camera1, const uint64_t seq1,
                                const unsigned long camera2, const uint64_t seq2)
   //-----------------------------------------------------------------------------
   {
      pair_hasher<unsigned long, uint64_t> hasher;
      size_t hash = hasher(std::make_pair(camera1, seq1));
      tbb::concurrent_hash_map<size_t , std::pair<unsigned long, uint64_t>>::accessor it;
      if (stereoFrames.find(it, hash))
      {
         it->second.first = camera2;
         it->second.second = seq2;
      }
      else
         stereoFrames.insert(std::make_pair(hash, std::make_pair(camera2, seq2)));
      hash = hasher(std::make_pair(camera2, seq2));
      if (stereoFrames.find(it, hash))
      {
         it->second.first = camera1;
         it->second.second = seq1;
      }
      else
         stereoFrames.insert(std::make_pair(hash, std::make_pair(camera1, seq1)));
   }

   bool Repository::stereo_twin(const unsigned long camera, const uint64_t seq,
                                unsigned long &twinCamera, uint64_t &twinSeq)
   //-------------------------------------------------------------------------
   {
      pair_hasher<unsigned long, uint64_t> hasher;
      size_t hash = hasher(std::make_pair(camera, seq));
      tbb::concurrent_hash_map<size_t , std::pair<unsigned long, uint64_t>>::accessor it;
      if (stereoFrames.find(it, hash))
      {
         twinCamera = it->second.first;
         twinSeq = it->second.second;
      }
      else
      {
         twinCamera = std::numeric_limits<unsigned long>::max();
         twinSeq = 0;
         return false;
      }
      return true;
   }

   bool Repository::delete_stereo_pair(const unsigned long camera, const uint64_t seq)
   //----------------------------------------------------------------------------------
   {
      pair_hasher<unsigned long, uint64_t> hasher;
      size_t hash = hasher(std::make_pair(camera, seq));
      tbb::concurrent_hash_map<size_t , std::pair<unsigned long, uint64_t>>::accessor it;
      if (stereoFrames.find(it, hash))
      {
         unsigned long twinCamera = it->second.first;
         uint64_t twinSeq = it->second.second;
         stereoFrames.erase(it);
         hash = hasher(std::make_pair(twinCamera, twinSeq));
         if (stereoFrames.find(it, hash))
            stereoFrames.erase(it);
         return true;
      }
      return false;
   }
}

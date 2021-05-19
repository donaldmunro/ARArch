#ifndef _MAR_ARCHITECTURE_H
#define _MAR_ARCHITECTURE_H

#include <vector>
#include <thread>
#include <functional>
#include <memory>
#include <sstream>

#include "tbb/flow_graph.h"
#include "tbb/task_scheduler_init.h"

#include "mar/architecture/tbb/TBBCalibration.h"
#include <mar/architecture/tbb/TBBRouter.h>
#include <mar/architecture/tbb/TBBDetector.h>
#include "mar/architecture/tbb/TBBTracker.h"
#include "mar/architecture/tbb/TBBRender.h"
#include "mar/render/Renderer.h"
#include "mar/architecture/tbb/TBBCameraSource.h"
#include "mar/CalibrationValues.hh"
#include <mar/acquisition/Sensors.h>

#include <android/log.h>

//#define TESTING_DETECT_TIME
#define DETECT_MEANRATE 400 //Poisson mean
//#define TESTING_TRACK_TIME
#define TRACK_MEANRATE 50

namespace toMAR
{
   enum class DetectorType : unsigned { NONE = 0, APRILTAGS = 1, FACE_RECOGNITION = 2, SIMULATE = 3 };

   enum class TrackerType : unsigned { NONE = 0, SIMULATE = 1 };

   enum class RendererType : unsigned { STANDARD = 0, BENCHMARK = 1, NONE = 3 };

   class FlowGraphArchitecture;

   FlowGraphArchitecture *make_architecture(std::string type, Renderer *renderer,
                                            std::vector<std::shared_ptr<Camera>> &rearCameras,
                                            std::vector<std::shared_ptr<Camera>> &frontCameras,
                                            DetectorType rearDetectorType, DetectorType frontDetectorType,
                                            TrackerType rearTrackerType, TrackerType frontTrackerType,
                                            RendererType rendererType = RendererType::STANDARD);

   class FlowGraphArchitecture
   //========================
   {
   public:
      explicit FlowGraphArchitecture(toMAR::Renderer* renderer, DetectorType detectorType,
                                     TrackerType trackerType,
                                     RendererType rendererType = RendererType::STANDARD) :
            repository(Repository::instance()),
            renderer(renderer), defaultDetectorType(detectorType),
            defaultTrackerType(trackerType), rendererType(rendererType)
                                     {}
      FlowGraphArchitecture(CalibrationValues calibration, DetectorType detectorType,
                            TrackerType trackerType) : repository(Repository::instance()),
                                                       defaultDetectorType(detectorType), defaultTrackerType(trackerType),
                                                       calibration(calibration) {}
      virtual ~FlowGraphArchitecture() = default;
      virtual bool start() = 0;
      virtual void stop() { repository->must_terminate.store(true); }

      static Detector* make_detector(DetectorType detectorType, unsigned long camera1,
                                     bool isOverlayOnRear =false,
                                     int64_t frequencyMs =0);
      static Detector* make_detector(DetectorType detectorType, unsigned long camera1,
                                     unsigned long camera2, bool isOverlayOnRear =false,
                                     int64_t frequencyMs =0);
      static Tracker* make_tracker(TrackerType trackerType, unsigned long camera1);
      static Tracker* make_tracker(TrackerType trackerType, unsigned long camera1,
                                   unsigned long camera2);
      static RenderNode* make_render(RendererType type, toMAR::Renderer* renderer,
                                     unsigned long camera, bool mustDelete =false);

   protected:
      Repository* repository;
      Renderer* renderer;
      DetectorType defaultDetectorType;
      TrackerType defaultTrackerType;
      RendererType rendererType;
      CalibrationValues calibration;

      void output_benchmark();
   };

   class TBBMonoArchitecture : public FlowGraphArchitecture
   //============================================================
   {
   public:
      TBBMonoArchitecture(Renderer* renderer, unsigned long camera, DetectorType detectorType,
                         TrackerType trackerType, RendererType rendererType = RendererType::STANDARD) :
            FlowGraphArchitecture(renderer, detectorType, trackerType, rendererType), cameraId(camera),
            cameraSourceNode(camera), renderNode(make_render(rendererType, renderer, cameraId))
      {}

      bool start() override;

      virtual ~TBBMonoArchitecture() = default;

   private:
      void run();
      std::thread thread;
      tbb::flow::graph graph;
      unsigned long cameraId;

      TBBMonoCameraSourceNode cameraSourceNode;
      tbb::flow::source_node<uintptr_t> tbbSourceNode{graph, cameraSourceNode, false};
      std::unique_ptr<Detector> detectorNode{make_detector(defaultDetectorType, cameraId)};
      std::unique_ptr<Tracker> trackerNode{make_tracker(defaultTrackerType, cameraId)};
      std::unordered_map<unsigned long, TBBRouterParameters> routerMap =
      {
         { cameraId, TBBRouterParameters(0, detectorNode.get(), trackerNode.get(), renderer, false) }
      };
      TBBRouter routerNode{routerMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbRouterNode{graph, tbb::flow::serial, routerNode};
//      TBBCalibration calibrationNode;
//      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbCalibrationNode{graph, 2, calibrationNode};
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbDetectorNode{graph, 1,
                                    [this] (uint64_t seqno) -> uint64_t { return (*detectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbTrackerNode{graph, 1,
                           [this] (uint64_t seqno) -> uint64_t { return (*trackerNode)(seqno); } };
      std::unique_ptr<RenderNode> renderNode;
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbRenderNode{graph, 1,
                           [this] (uint64_t seqno) -> uint64_t { return (*renderNode)(seqno); } };
   };

   class TBBStereoArchitecture : public FlowGraphArchitecture
   //============================================================
   {
   public:
      TBBStereoArchitecture(Renderer* renderer, unsigned long camera1, unsigned long camera2,
                            DetectorType detectorType, TrackerType trackerType,
                            RendererType rendererType = RendererType::STANDARD) :
            FlowGraphArchitecture(renderer, detectorType, trackerType),
            cameraId1(camera1), cameraId2(camera2),
            cameraSourceNode1(camera1), cameraSourceNode2(camera2),
            renderNode(make_render(rendererType, renderer, camera1))
      {}

      bool start() override
      //-------------------
      {
         thread = std::thread(&TBBStereoArchitecture::run, this);
         return true;
      }

      virtual ~TBBStereoArchitecture() = default;

   private:
      void run();
      std::thread thread;
      tbb::flow::graph graph;
      unsigned long cameraId1, cameraId2;

      TBBMonoCameraSourceNode cameraSourceNode1, cameraSourceNode2;
      tbb::flow::source_node<uintptr_t> tbbSourceNode1{graph, cameraSourceNode1, false};
      tbb::flow::source_node<uintptr_t> tbbSourceNode2{graph, cameraSourceNode2, false};
      tbb::flow::join_node<std::tuple<uintptr_t, uintptr_t>> tbbCamerasJoinNode{graph};
      tbb::flow::function_node<std::tuple<uintptr_t, uintptr_t>, uintptr_t, tbb::flow::queueing>
      tbbJoinProcessor{graph, 1,
      [this] (std::tuple<uintptr_t, uintptr_t> tt) -> uintptr_t
      //--------------------------------------------------------
      {
         CameraFrame* cameraFrame1 = (CameraFrame*) std::get<0>(tt);
         CameraFrame* cameraFrame2 = (CameraFrame*) std::get<1>(tt);
         if ( (cameraFrame1->count + cameraFrame2->count) == 0)
            return (uintptr_t) cameraFrame1;
         // __android_log_print(ANDROID_LOG_INFO, " Stereo join node", "Router %lu %lu %lu %lu",
         //                     cameraFrame1->cameras[0].cameraId, cameraFrame1->cameras[0].seqno_1,
         //                     cameraFrame2->cameras[0].cameraId, cameraFrame2->cameras[0].seqno_1);
         unsigned long id1 = cameraFrame1->cameras[0].cameraId;
         uint64_t seq1 = cameraFrame1->cameras[0].seqno_1;
         unsigned long id2 = cameraFrame2->cameras[0].cameraId;
         uint64_t seq2 = cameraFrame2->cameras[0].seqno_1;
         repository->stereo_pair(id1, seq1, id2, seq2);
         cameraFrame1->set_stereo(0, id1, id2, seq1, seq2, true);
         return (uintptr_t) cameraFrame1;
      } };
      std::unique_ptr<Detector> detectorNode{make_detector(defaultDetectorType, cameraId1, cameraId2)};
      std::unique_ptr<Tracker> trackerNode{make_tracker(defaultTrackerType, cameraId1, cameraId2)};
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbDetectorNode{graph, 1,
                           [this] (uint64_t seqno) -> uint64_t { return (*detectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbTrackerNode{graph, 1,
                           [this] (uint64_t seqno) -> uint64_t { return (*trackerNode)(seqno); } };
      std::unordered_map<unsigned long, TBBRouterParameters> routerMap =
      {
         { cameraId1, TBBRouterParameters(0, detectorNode.get(), trackerNode.get(), renderer, false) },
      };
      TBBRouter routerNode{routerMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbRouterNode{graph, 2, routerNode};
      std::unique_ptr<RenderNode> renderNode;
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbRenderNode{graph, 1,
                             [this] (uint64_t seqno) -> uint64_t { return (*renderNode)(seqno); } };
   };

   //Monocular rear and front with rendering of rear + overlayed face detected in front camera
   class TBBMonoAndFrontArchitecture : public FlowGraphArchitecture
   //============================================================
   {
   public:
      TBBMonoAndFrontArchitecture(Renderer* renderer, unsigned long backCamera,
                                  unsigned long frontCamera,
                                  DetectorType rearDetectorType, DetectorType frontDetectorType,
                                  TrackerType rearTrackerType, TrackerType frontTrackerType,
                                  RendererType rendererType = RendererType::STANDARD) :
            FlowGraphArchitecture(renderer, rearDetectorType, rearTrackerType),
            backCameraId(backCamera), frontCameraId(frontCamera), frontDetectorType(frontDetectorType),
            backSourceNode(backCamera), frontSourceNode(frontCamera),
            renderNode(make_render(rendererType, renderer, backCamera))//,
//            dummyRenderNode(make_render(RendererType::NONE, nullptr, frontCamera, true))
      {}


      bool start() override;

      virtual ~TBBMonoAndFrontArchitecture() = default;

   private:
      void run();
      std::thread thread;
      tbb::flow::graph graph;
      unsigned long backCameraId, frontCameraId;
      DetectorType frontDetectorType;

      TBBMonoCameraSourceNode backSourceNode, frontSourceNode;
      tbb::flow::source_node<uintptr_t> tbbBackSourceNode{graph, backSourceNode, false};
      tbb::flow::source_node<uintptr_t> tbbFrontSourceNode{graph, frontSourceNode, false};
      std::unique_ptr<Detector> backDetectorNode{make_detector(defaultDetectorType, backCameraId)};
      std::unique_ptr<Tracker> backTrackerNode{make_tracker(defaultTrackerType, backCameraId)};
      std::unique_ptr<Detector> frontDetectorNode{make_detector(frontDetectorType, frontCameraId,
                                                  true, true, 0)};
      std::unique_ptr<Tracker> frontTrackerNode{make_tracker(defaultTrackerType, frontCameraId, true)};
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbFrontDetectorNode{graph, 1,
         [this] (uint64_t seqno) -> uint64_t { return (*frontDetectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbFrontTrackerNode{graph, 1,
         [this] (uint64_t seqno) -> uint64_t { return (*frontTrackerNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbBackDetectorNode{graph, 1,
         [this] (uint64_t seqno) -> uint64_t { return (*backDetectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbBackTrackerNode{graph, 1,
         [this] (uint64_t seqno) -> uint64_t { return (*backTrackerNode)(seqno); } };
      std::unordered_map<unsigned long, TBBRouterParameters> rearRouterMap =
      {
         { backCameraId, TBBRouterParameters(0, backDetectorNode.get(), backTrackerNode.get(), renderer, false) },
      };
      TBBRouter rearRouterNode{rearRouterMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbRearRouterNode{graph, 1, rearRouterNode};
      std::unordered_map<unsigned long, TBBRouterParameters> frontRouterMap =
      {
         { frontCameraId, TBBRouterParameters(0, frontDetectorNode.get(), frontTrackerNode.get(), nullptr, true) }
      };
      TBBRouter frontRouterNode{frontRouterMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbFrontRouterNode{graph, 1, frontRouterNode};
      std::unique_ptr<RenderNode> renderNode;
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbRenderNode{graph, 1,
         [this] (uint64_t seqno) -> uint64_t { return (*renderNode)(seqno); } };
//      std::unique_ptr<RenderNode> dummyRenderNode;
//      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbDummyRenderNode{graph, 1,
//         [this] (uint64_t seqno) -> uint64_t { return (*dummyRenderNode)(seqno); } };
   };

   /*
    * WARNING: TBBStereoAndFrontArchitecture untested as none of my devices supported
    * 3 video streams.
    */
   //Stereo rear and monocular front with rendering of rear and overlay face detected in front camera
   class TBBStereoAndFrontArchitecture : public FlowGraphArchitecture
      //============================================================
   {
   public:
      TBBStereoAndFrontArchitecture(Renderer* renderer, unsigned long backCamera1,
                                    unsigned long backCamera2, unsigned long frontCamera,
                                    DetectorType rearDetectorType, DetectorType frontDetectorType,
                                    TrackerType rearTrackerType, TrackerType frontTrackerType,
                                    RendererType rendererType = RendererType::STANDARD) :
            FlowGraphArchitecture(renderer, rearDetectorType, rearTrackerType),
            backCameraId1(backCamera1), backCameraId2(backCamera2),
            frontCameraId(frontCamera), frontDetectorType(frontDetectorType),
            backCameraSourceNode1(backCamera1), backCameraSourceNode2(backCamera2),
            frontCameraSourceNode(frontCamera),
            renderNode(make_render(rendererType, renderer, backCamera1))
      {}

      bool start() override;

      virtual ~TBBStereoAndFrontArchitecture() = default;

   private:
      void run();
      std::thread thread;
      tbb::flow::graph graph;
      unsigned long backCameraId1, backCameraId2, frontCameraId;
      DetectorType frontDetectorType;

      TBBMonoCameraSourceNode backCameraSourceNode1, backCameraSourceNode2, frontCameraSourceNode;
      tbb::flow::source_node<uintptr_t> tbbSourceNode1{graph, backCameraSourceNode1, false};
      tbb::flow::source_node<uintptr_t> tbbSourceNode2{graph, backCameraSourceNode2, false};
      tbb::flow::source_node<uintptr_t> tbbFrontSourceNode{graph, frontCameraSourceNode, false};
      tbb::flow::join_node<std::tuple<uintptr_t, uintptr_t>> tbbCamerasJoinNode{graph};
      tbb::flow::function_node<std::tuple<uintptr_t, uintptr_t>, uintptr_t, tbb::flow::queueing>
            tbbJoinProcessor{graph, 1,
      [this] (std::tuple<uintptr_t, uintptr_t> tt) -> uintptr_t
      //--------------------------------------------------------
      {
         CameraFrame* cameraFrame1 = (CameraFrame*) std::get<0>(tt);
         CameraFrame* cameraFrame2 = (CameraFrame*) std::get<1>(tt);
         if ( (cameraFrame1->count + cameraFrame2->count) == 0)
            return (uintptr_t) cameraFrame1;
         // __android_log_print(ANDROID_LOG_INFO, " Stereo join node", "Router %lu %lu %lu %lu",
         //                     cameraFrame1->cameras[0].cameraId, cameraFrame1->cameras[0].seqno_1,
         //                     cameraFrame2->cameras[0].cameraId, cameraFrame2->cameras[0].seqno_1);
         unsigned long id1 = cameraFrame1->cameras[0].cameraId;
         uint64_t seq1 = cameraFrame1->cameras[0].seqno_1;
         unsigned long id2 = cameraFrame2->cameras[0].cameraId;
         uint64_t seq2 = cameraFrame2->cameras[0].seqno_1;
         repository->stereo_pair(id1, seq1, id2, seq2);
         cameraFrame1->set_stereo(0, id1, id2, seq1, seq2, true);
         return (uintptr_t) cameraFrame1;
      } };
      std::unique_ptr<Detector> backDetectorNode{make_detector(defaultDetectorType, backCameraId1, backCameraId2)};
      std::unique_ptr<Tracker> backTrackerNode{make_tracker(defaultTrackerType, backCameraId1, backCameraId2)};
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbBackDetectorNode{graph, 1,
      [this] (uint64_t seqno) -> uint64_t { return (*backDetectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbBackTrackerNode{graph, 1,
      [this] (uint64_t seqno) -> uint64_t { return (*backTrackerNode)(seqno); } };
      std::unique_ptr<Detector> frontDetectorNode{make_detector(frontDetectorType, frontCameraId,
                                                  true, true, 0)};
      std::unique_ptr<Tracker> frontTrackerNode{make_tracker(defaultTrackerType, frontCameraId, true)};
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbFrontDetectorNode{graph, 1,
      [this] (uint64_t seqno) -> uint64_t { return (*frontDetectorNode)(seqno); } };
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbFrontTrackerNode{graph, 1,
      [this] (uint64_t seqno) -> uint64_t { return (*frontTrackerNode)(seqno); } };
      std::unordered_map<unsigned long, TBBRouterParameters> backRouterMap =
      {
         { backCameraId1, TBBRouterParameters(0, backDetectorNode.get(), backTrackerNode.get(), renderer, false) },
      };
      TBBRouter backRouterNode{backRouterMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbBackRouterNode{graph, 2, backRouterNode};
      std::unordered_map<unsigned long, TBBRouterParameters> frontRouterMap =
      {
         { frontCameraId, TBBRouterParameters(0, frontDetectorNode.get(), frontTrackerNode.get(), nullptr, true) }
      };
      TBBRouter frontRouterNode{frontRouterMap};
      tbb::flow::multifunction_node<uintptr_t, RouterOutputTuple> tbbFrontRouterNode{graph, 1, frontRouterNode};
      std::unique_ptr<RenderNode> renderNode;
      tbb::flow::function_node<uint64_t, uint64_t, tbb::flow::rejecting> tbbRenderNode{graph, 1,
      [this] (uint64_t seqno) -> uint64_t { return (*renderNode)(seqno); } };
   };
};
#endif

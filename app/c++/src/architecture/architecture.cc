#include "mar/architecture/Architecture.h"

namespace toMAR
{
   FlowGraphArchitecture *make_architecture(std::string type, Renderer *renderer,
                                            std::vector<std::shared_ptr<Camera>> &rearCameras,
                                            std::vector<std::shared_ptr<Camera>> &frontCameras,
                                            DetectorType rearDetectorType, DetectorType frontDetectorType,
                                            TrackerType rearTrackerType, TrackerType frontTrackerType,
                                            RendererType rendererType)
   //-----------------------------------------------------------------------------------------
   {
      if (type == "TBB")
      {
         if ( (! rearCameras.empty()) && (! frontCameras.empty()) )
         {
            if (rearCameras.size() > 1)
               return new TBBStereoAndFrontArchitecture(renderer, rearCameras[0]->camera_id(),
                                                        rearCameras[1]->camera_id(),
                                                        frontCameras[0]->camera_id(),
                                                        rearDetectorType, frontDetectorType,
                                                        rearTrackerType, frontTrackerType,
                                                        rendererType);
            else
               return new TBBMonoAndFrontArchitecture(renderer, rearCameras[0]->camera_id(),
                                                      frontCameras[0]->camera_id(),
                                                      rearDetectorType, frontDetectorType,
                                                      rearTrackerType, frontTrackerType,
                                                      rendererType);
         }
         else if (! rearCameras.empty())
         {
            if (rearCameras.size() > 1)
               return new TBBStereoArchitecture(renderer, rearCameras[0]->camera_id(),
                                                rearCameras[1]->camera_id(), rearDetectorType,
                                                rearTrackerType, rendererType);
            else
               return new TBBMonoArchitecture(renderer, rearCameras[0]->camera_id(), rearDetectorType,
                                              rearTrackerType, rendererType);
         }
         else if (! frontCameras.empty())
            return new TBBMonoArchitecture(renderer, frontCameras[0]->camera_id(), frontDetectorType,
                                           frontTrackerType, rendererType);
      }
      return nullptr;
   }

   Detector *FlowGraphArchitecture::make_detector(DetectorType detectorType, unsigned long camera1,
                                                  bool isOverlayOnRear, int64_t frequencyMs)
   //----------------------------------------------------------------------------------------------
   {
      return make_detector(detectorType, camera1, std::numeric_limits<unsigned long>::max(),
                           isOverlayOnRear, frequencyMs);
   }

   Detector* FlowGraphArchitecture::make_detector(DetectorType detectorType, unsigned long camera1,
                                                  unsigned long camera2, bool isOverlayOnRear,
                                                  int64_t frequencyMs)
   //-----------------------------------------------------------------------------------------
   {
      switch (detectorType)
      {
         case DetectorType::SIMULATE:
            return new TBBSimulationDetector(camera1, DETECT_MEANRATE);
         case DetectorType::APRILTAGS:
            return new AprilTagTBBDetector(camera1, camera2);
         case DetectorType::FACE_RECOGNITION:
            if (isOverlayOnRear)
               return new FaceOverlayTBBDetector(camera1);
            else
               return new FaceTBBDetector(camera1);
         case DetectorType::NONE: return new TBBNullDetector(camera1);
      }
   }

   Tracker* FlowGraphArchitecture::make_tracker(TrackerType trackerType, unsigned long camera1)
   {
      return make_tracker(trackerType, camera1, std::numeric_limits<unsigned long>::max());
   }

   Tracker* FlowGraphArchitecture::make_tracker(TrackerType trackerType, unsigned long camera1,
                                                unsigned long camera2)
   //----------------------------------------------------------------------------------------
   {
      switch (trackerType)
      {
         case TrackerType::SIMULATE:
            return new TBBTimeTestTracker(camera1, TRACK_MEANRATE);
         case TrackerType::NONE:
            return new TBBNullTracker(camera1);
      }
   }

   RenderNode *FlowGraphArchitecture::make_render(RendererType type, toMAR::Renderer* renderer,
                                                  unsigned long camera, bool mustDelete)
   //-----------------------------------------------------------------------------------------
   {
		if (renderer == nullptr)
         type = RendererType::NONE;
		switch (type)
      {
         case RendererType::STANDARD:
            return new TBBRender(renderer, camera);
         case RendererType::BENCHMARK:
            return new TBBBenchmarkRender(renderer, camera);
         case RendererType::NONE:
            return new TBBNullRenderer(camera, mustDelete);
      }
      return nullptr;
   }

   void TBBMonoArchitecture::run()
   //----------------------------------
   {
      tbb::task_scheduler_init init_parallel;
      uint64_t tc = static_cast<uint64_t>(init_parallel.default_num_threads());
      tbb::flow::make_edge(tbbSourceNode, tbbRouterNode );
//      tbb::flow::output_port<0>(tbbRouterNode).register_successor(tbbCalibrationNode);
//      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbCalibrationNode);
      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbRouterNode), tbbDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbTrackerNode);
      tbb::flow::make_edge(tbb::flow::output_port<2>(tbbRouterNode), tbbRenderNode);
//      tbb::flow::make_edge(tbbCalibrationNode, tbbRenderNode);
//      tbb::flow::make_edge(tbbTrackerNode, tbbRenderNode);

      bool stopSensors = false, isSingleThreadedRender = renderer->is_single_threaded();
      Sensors& sensorController = Sensors::instance();
      size_t noSensors = sensorController.size();
      std::thread sensorThread;
      if ( (isSingleThreadedRender) && (noSensors > 0) )
         sensorThread = std::thread(&Sensors::sensor_handler_thread, &sensorController, 10, 200, std::ref(stopSensors));
      else if (! isSingleThreadedRender)
      {
         std::stringstream errs;
         size_t inited = sensorController.initialize(&errs);
         if (inited != noSensors)
         {
            __android_log_print(ANDROID_LOG_ERROR, "TBBFlowGraphArchitecture::run()",
                                "Not all sensors initialized (%s %ld/%ld)", errs.str().c_str(), inited, noSensors);
            noSensors = inited;
         }
      }

      renderer->initialize();
      repository->initialised.store(true);
      tbbSourceNode.activate();
      while ( (! graph.is_cancelled()) && (! repository->must_terminate.load()) )
      {
         if (isSingleThreadedRender)
         {
            uint64_t seqno = renderer->render_st();
            if (seqno > 0)
               ;//tbbFramerateControlQueueNode.try_put(seqno);
            else
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else if (noSensors > 0)
            sensorController.process_queues(10, 200);
         else
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

      }
//    if (detectorType == DetectorType::BENCHMARK)
      output_benchmark();

      if ( (isSingleThreadedRender) && (noSensors > 0) )
      {
         stopSensors = true;
         sensorThread.join();
      }


   }

   bool TBBMonoArchitecture::start()
   //------------------------------
   {
      if (! cameraSourceNode.good())
      {
         __android_log_print(ANDROID_LOG_ERROR, "TBBMonoArchitecture::start()",
                             "Error initialising camera source");
         return false;
      }
      thread = std::thread(&TBBMonoArchitecture::run, this);
      return true;
   }

   void TBBStereoArchitecture::run()
   //----------------------------------
   {
      tbb::task_scheduler_init init_parallel;
      uint64_t tc = static_cast<uint64_t>(init_parallel.default_num_threads());

      tbb::flow::make_edge(tbbSourceNode1, tbb::flow::input_port<0>(tbbCamerasJoinNode));
      tbb::flow::make_edge(tbbSourceNode2, tbb::flow::input_port<1>(tbbCamerasJoinNode));
      tbb::flow::make_edge(tbbCamerasJoinNode,  tbbJoinProcessor);
      tbb::flow::make_edge(tbbJoinProcessor,  tbbRouterNode);
//      tbb::flow::make_edge(tbbCamerasJoinNode, tbbRouterNode );

//      tbb::flow::output_port<0>(tbbRouterNode).register_successor(tbbCalibrationNode);
//      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbCalibrationNode);
      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbRouterNode), tbbDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbTrackerNode);
      tbb::flow::make_edge(tbb::flow::output_port<2>(tbbRouterNode), tbbRenderNode);
//      tbb::flow::make_edge(tbbCalibrationNode, tbbRenderNode);
//      tbb::flow::make_edge(tbbTrackerNode, tbbRenderNode);

      bool stopSensors = false, isSingleThreadedRender = renderer->is_single_threaded();
      Sensors& sensorController = Sensors::instance();
      size_t noSensors = sensorController.size();
      std::thread sensorThread;
      if ( (isSingleThreadedRender) && (noSensors > 0) )
         sensorThread = std::thread(&Sensors::sensor_handler_thread, &sensorController, 10, 200, std::ref(stopSensors));
      else if (! isSingleThreadedRender)
      {
         std::stringstream errs;
         size_t inited = sensorController.initialize(&errs);
         if (inited != noSensors)
         {
            __android_log_print(ANDROID_LOG_ERROR, "TBBFlowGraphArchitecture::run()",
                                "Not all sensors initialized (%s %ld/%ld)", errs.str().c_str(), inited, noSensors);
            noSensors = inited;
         }
      }

      renderer->initialize();
      tbbSourceNode1.activate(); tbbSourceNode2.activate();
      repository->initialised.store(true);
      while ( (! graph.is_cancelled()) && (! repository->must_terminate) )
      {
         if (isSingleThreadedRender)
         {
            uint64_t seqno = renderer->render_st();
            if (seqno > 0)
               ;//tbbFramerateControlQueueNode.try_put(seqno);
            else
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else if (noSensors > 0)
            sensorController.process_queues(10, 200);
         else
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

      }
//    if (detectorType == DetectorType::BENCHMARK)
      output_benchmark();
      if ( (isSingleThreadedRender) && (noSensors > 0) )
      {
         stopSensors = true;
         sensorThread.join();
      }

   }

   bool TBBMonoAndFrontArchitecture::start()
   //--------------------------------------
   {
      thread = std::thread(&TBBMonoAndFrontArchitecture::run, this);
//      frontThread = std::thread(&TBBMonoAndFrontArchitecture::run_forward, this);
      return true;
   }

   void TBBMonoAndFrontArchitecture::run()
   //------------------------------------------
   {
      tbb::flow::make_edge(tbbBackSourceNode, tbbRearRouterNode );
      tbb::flow::make_edge(tbbFrontSourceNode, tbbFrontRouterNode );
//      tbb::flow::output_port<0>(tbbRouterNode).register_successor(tbbCalibrationNode);
//      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbCalibrationNode);
      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbRearRouterNode), tbbBackDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRearRouterNode), tbbBackTrackerNode);
      tbb::flow::make_edge(tbb::flow::output_port<2>(tbbRearRouterNode), tbbRenderNode);

      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbFrontRouterNode), tbbFrontDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbFrontRouterNode), tbbFrontTrackerNode);
//      tbb::flow::make_edge(tbb::flow::output_port<2>(tbbFrontRouterNode), tbbDummyRenderNode);

      bool stopSensors = false, isSingleThreadedRender = renderer->is_single_threaded();
      Sensors& sensorController = Sensors::instance();
      size_t noSensors = sensorController.size();
      std::thread sensorThread;
      if ( (isSingleThreadedRender) && (noSensors > 0) )
         sensorThread = std::thread(&Sensors::sensor_handler_thread, &sensorController, 10, 200, std::ref(stopSensors));
      else if (! isSingleThreadedRender)
      {
         std::stringstream errs;
         size_t inited = sensorController.initialize(&errs);
         if (inited != noSensors)
         {
            __android_log_print(ANDROID_LOG_ERROR, "TBBFlowGraphArchitecture::run()",
                                "Not all sensors initialized (%s %ld/%ld)", errs.str().c_str(), inited, noSensors);
            noSensors = inited;
         }
      }

      renderer->initialize();
      tbbBackSourceNode.activate(); tbbFrontSourceNode.activate();
      repository->initialised.store(true);
      while ( (! repository->must_terminate) && (! graph.is_cancelled()) )
      {
         if (isSingleThreadedRender)
         {
            uint64_t seqno = renderer->render_st();
            //if (seqno > 0)
               ;//tbbFramerateControlQueueNode.try_put(seqno);
            //else
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else if (noSensors > 0)
            sensorController.process_queues(10, 200);
         else
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      if ( (isSingleThreadedRender) && (noSensors > 0) )
      {
         stopSensors = true;
         sensorThread.join();
      }
      graph.wait_for_all();
//      if (detectorType == DetectorType::BENCHMARK)
         output_benchmark();
   }

   /*
    * WARNING: TBBStereoAndFrontArchitecture untested as none of my devices supported
    * 3 video streams.
    */
   bool TBBStereoAndFrontArchitecture::start()
   //-----------------------------------------
   {
      thread = std::thread(&TBBStereoAndFrontArchitecture::run, this);
      return true;
   }

   void TBBStereoAndFrontArchitecture::run()
   //----------------------------------
   {
      tbb::task_scheduler_init init_parallel;
      uint64_t tc = static_cast<uint64_t>(init_parallel.default_num_threads());

      tbb::flow::make_edge(tbbSourceNode1, tbb::flow::input_port<0>(tbbCamerasJoinNode));
      tbb::flow::make_edge(tbbSourceNode2, tbb::flow::input_port<1>(tbbCamerasJoinNode));
      tbb::flow::make_edge(tbbFrontSourceNode, tbbFrontRouterNode );
      tbb::flow::make_edge(tbbCamerasJoinNode,  tbbJoinProcessor);
      tbb::flow::make_edge(tbbJoinProcessor,  tbbBackRouterNode);
//      tbb::flow::make_edge(tbbCamerasJoinNode, tbbRouterNode );

//      tbb::flow::output_port<0>(tbbRouterNode).register_successor(tbbCalibrationNode);
//      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbRouterNode), tbbCalibrationNode);
      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbBackRouterNode), tbbBackDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbBackRouterNode), tbbBackTrackerNode);
      tbb::flow::make_edge(tbb::flow::output_port<2>(tbbBackRouterNode), tbbRenderNode);
      tbb::flow::make_edge(tbb::flow::output_port<0>(tbbFrontRouterNode), tbbFrontDetectorNode);
      tbb::flow::make_edge(tbb::flow::output_port<1>(tbbFrontRouterNode), tbbFrontTrackerNode);

      bool stopSensors = false, isSingleThreadedRender = renderer->is_single_threaded();
      Sensors& sensorController = Sensors::instance();
      size_t noSensors = sensorController.size();
      std::thread sensorThread;
      if ( (isSingleThreadedRender) && (noSensors > 0) )
         sensorThread = std::thread(&Sensors::sensor_handler_thread, &sensorController, 10, 200, std::ref(stopSensors));
      else if (! isSingleThreadedRender)
      {
         std::stringstream errs;
         size_t inited = sensorController.initialize(&errs);
         if (inited != noSensors)
         {
            __android_log_print(ANDROID_LOG_ERROR, "TBBFlowGraphArchitecture::run()",
                                "Not all sensors initialized (%s %ld/%ld)", errs.str().c_str(), inited, noSensors);
            noSensors = inited;
         }
      }

      renderer->initialize();
      tbbSourceNode1.activate(); tbbSourceNode2.activate();
      tbbFrontSourceNode.activate();
      repository->initialised.store(true);
      while ( (! graph.is_cancelled()) && (! repository->must_terminate) )
      {
         if (isSingleThreadedRender)
         {
            uint64_t seqno = renderer->render_st();
            if (seqno > 0)
               ;//tbbFramerateControlQueueNode.try_put(seqno);
            else
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else if (noSensors > 0)
            sensorController.process_queues(10, 200);
         else
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

      }
      if ( (isSingleThreadedRender) && (noSensors > 0) )
      {
         stopSensors = true;
         sensorThread.join();
      }
//      if (detectorType == DetectorType::BENCHMARK)
         output_benchmark();
   }

   void FlowGraphArchitecture::output_benchmark()
   //-------------------------------------------
   {
      std::ofstream ofs("/sdcard/detector-stats.txt");
      if (ofs)
      {
         for (auto it = repository->detectorStatistics.begin();
                   it != repository->detectorStatistics.end(); ++it)
         {
            unsigned long camera = it->first;
            RunningStatistics<uint64_t, long double>* statistics = it->second;
            constexpr long double nano2s = static_cast<long double>(1000000000.0);
            if ( (camera != std::numeric_limits<unsigned long>::max()) && (statistics != nullptr) )
            {
               ofs << "Camera " << camera << std::endl;
               long double mean = statistics->mean();
               ofs << "Mean: " << std::fixed << std::setprecision(8) << mean / nano2s << std::endl;
               ofs << "Deviation:" << std::fixed << std::setprecision(8) << statistics->deviation() / nano2s << std::endl;
               ofs << "Framerate (mean): " << std::fixed << std::setprecision(8) << (nano2s / mean) << std::endl;
               ofs << std::string(80, '=') << std::endl;
            }
         }
         ofs.close();
      }
   }
}

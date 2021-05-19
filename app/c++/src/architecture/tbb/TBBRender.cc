#include "mar/architecture/tbb/TBBRender.h"

namespace toMAR
{
   uint64_t TBBRender::operator()(uint64_t seqno) const
   //--------------------------------------------------
   {
      if ( (seqno == 0) || (! renderer->is_initialized()) || (repository->must_terminate.load()) )
      {
//            if (! renderer->is_single_threaded())
//               renderer->initialize(rendererParams);
//            else //don't initialize on another thread
         repository->delete_frame(cameraId, seqno);
         return seqno;
      }
#ifdef DEBUG_SAVE_TEXTURE
      if (debugSaveThread != nullptr)
      {
         debugSaveThread->join();
         delete debugSaveThread;
         debugSaveThread = nullptr;
      }
      debugSaveThread = new std::thread(TBBRender::debug_threadproc, seqno);
#endif
      if (renderer->is_single_threaded())
      {
         renderer->render(seqno, cameraId);
         return seqno;
      }
      //TODO: Double buffering for high (> 30) frame rates ?
      bool rendering = false;
      if (repository->is_rendering.compare_exchange_strong(rendering, true))
      {
         renderer->render(seqno, cameraId); //TODO CHECK: renderer please reset repository->isRendering when done
         //if (renderer->is_single_threaded()) while (repository->isRendering.load(std::memory_order_acquire));
      }
      else
         repository->delete_frame(cameraId, seqno);
      return seqno;
   }

#ifdef DEBUG_SAVE_TEXTURE
   std::thread* TBBRender::debugSaveThread = nullptr;

   void TBBRender::debug_threadproc(uint64_t seqno)
   {
      Repository* repo = Repository::instance();
      std::shared_ptr<FrameInfo> frame;
      if ( (repo->get_frame(0, seqno, frame)) && (frame) )
      {
         std::stringstream ss;
         ss << "/sdcard/Pictures/" << seqno << ".png";
         cv::imwrite(ss.str().c_str(), *frame->rgba);
//         ss.str("");
//         ss << "/sdcard/Pictures/" << seqno << ".jpg";
//         stbi_write_jpg(ss.str().c_str(), frame->width, frame->height, 1, frame->frame_data, 9);
         if (seqno > 100)
         {
//            ss.str("");
//            ss << "/sdcard/Pictures/" << (seqno - 100) << ".jpg";
//            ::remove(ss.str().c_str());
            ss.str("");
            ss << "/sdcard/Pictures/" << (seqno - 100) << ".png";
            ::remove(ss.str().c_str());
         }
      }
   }
#endif

   int64_t TBBBenchmarkRender::last_timestamp = util::now_monotonic();

   uint64_t TBBBenchmarkRender::operator()(uint64_t seqno) const
   //-----------------------------------------------------------
   {
      if ( (seqno == 0) || (! renderer->is_initialized()) || (repository->must_terminate.load()) )
      {
         repository->delete_frame(cameraId, seqno);
         return seqno;
      }
      if (renderer->is_single_threaded())
      {
         renderer->render(seqno, cameraId); // Single threaded render places on queue for rendering in context thread
         return seqno;
      }
      bool rendering = false;
      if (repository->is_rendering.compare_exchange_strong(rendering, true))
      {
         std::shared_ptr<FrameInfo> frame;
         if ( (repository->get_frame(cameraId, seqno, frame)) && (frame) )
         {
            __android_log_print(ANDROID_LOG_INFO, " TBBBenchmarkRender::operator()", "Render %lu %lu", frame->camera_id, frame->seqno);
            const int64_t ts = frame->timestamp;
            RunningStatistics<uint64_t, long double>* statistics = repository->rendererStats(cameraId);
            if (statistics)
            {
               long double taken = static_cast<long double>(ts - last_timestamp);
               (*statistics)(taken);
            }
            last_timestamp = ts;
            renderer->render(seqno, cameraId); //TODO CHECK: renderer please reset repository->isRendering when done
//            __android_log_print(ANDROID_LOG_INFO, "TBBBenchmarkRender::()", "Rendered seqno %lu for camera %lu renderer %s", seqno, cameraId, renderer->name());
         }
         else
         {
            repository->is_rendering.store(false);
            __android_log_print(ANDROID_LOG_WARN, "TBBBenchmarkRender::()", "Could not find seqno %lu for camera %lu renderer %s", seqno, cameraId, renderer->name());
         }
      }
      else
      {
         __android_log_print(ANDROID_LOG_WARN, "TBBBenchmarkRender::()", "Could not set rendering flag seq %lu camera %lu renderer %s", seqno, cameraId, renderer->name());
         repository->delete_frame(cameraId, seqno);
      }
      return seqno;
   }

   uint64_t TBBNullRenderer::operator()(uint64_t seqno) const
   //--------------------------------------------------------
   {
      if (mustDelete)
         repository->delete_frame(cameraId, seqno);
      return seqno;
   }
}

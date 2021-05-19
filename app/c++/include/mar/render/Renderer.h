#ifndef _MAR_RENDERER_H
#define _MAR_RENDERER_H

#include <cstdio>
#include <utility>
#include <memory>
#include <chrono>
#include <functional>

#include "mar/Repository.h"
#include "mar/acquisition/FrameInfo.h"

using TimeType = std::chrono::time_point<std::chrono::high_resolution_clock>; //std::chrono::system_clock::time_point;

namespace toMAR
{
   class Renderer
   //============
   {
   public:
      explicit Renderer(const char *appName =nullptr, const char* assetsDir =nullptr, bool isShowFPS =false) :
            repository(Repository::instance()), appName(appName == nullptr ? "" : appName),
            assetDir(assetsDir == nullptr ? "" : std::string(assetsDir)), is_fps(isShowFPS),
            last_timestamp(std::chrono::high_resolution_clock::now())
      {}

       //For renderers which need to be initialized on a specific thread
      virtual bool is_initialized() { return true; }
      virtual bool is_single_threaded() =0;
      virtual void set_initialization_parameters(void *initParameters) {}
      virtual bool initialize(void *initializationParameters = nullptr) { return true; }

      virtual bool render(uint64_t seqno, unsigned long cameraNo =0) =0;
      virtual uint64_t render_st() { return 0; };

      virtual void rendered(uint64_t seqno, int cameraNo =0) { }
      virtual const char* name() =0;

      virtual ~Renderer() {}

   protected:
      inline void update_fps()
      //----------------------
      {
         cframes++;
         TimeType timestamp = std::chrono::high_resolution_clock::now();
         long elapsed = std::chrono::duration_cast<std::chrono::seconds>(timestamp - last_timestamp).count();
         if (elapsed >= 1)//1000000000L)
         {
            fps = static_cast<int>(cframes/elapsed);
            cframes = 0;
            last_timestamp = timestamp;
   //         std::cout << fps << " " << elapsed << std::endl;
         }
      }

      Repository* repository;
      std::string appName, assetDir;
      bool is_fps = false;
      size_t cframes = 0;
      int fps = -1;
      TimeType last_timestamp;
//      float background_color[4] = { 0.0f, 0.34f, 0.90f, 1.0f };
      float background_color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
   };
};
#endif

#ifndef _MAR_RENDERER_NODE_H
#define _MAR_RENDERER_NODE_H

#include <memory>
#include <tuple>

#include "tbb/flow_graph.h"

#include "mar/Repository.h"
#include "mar/render/RendererFactory.hh"
#include "mar/acquisition/FrameInfo.h"
#include <mar/util/Countable.hh>

//#define DEBUG_SAVE_TEXTURE
#ifdef DEBUG_SAVE_TEXTURE
#include <thread>
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb/stb_image_write.h"
#include "opencv2/imgcodecs.hpp"
#endif

namespace toMAR
{
   class RenderNode
   {
   public:
      virtual uint64_t operator()(uint64_t seqno) const =0;

      virtual ~RenderNode() {}
   };

   class TBBRender : public RenderNode
   //=================================
   {
   public:
      explicit TBBRender(toMAR::Renderer* renderer, unsigned long camera) :
            repository(Repository::instance()), cameraId(camera), renderer(renderer)
      {}

      uint64_t operator()(uint64_t seqno) const override ;

   protected:
      Repository* repository;
      const unsigned long cameraId;
      std::shared_ptr<toMAR::Renderer> renderer;

#ifdef DEBUG_SAVE_TEXTURE
   private:
      static std::thread* debugSaveThread;
      static void debug_threadproc(uint64_t seqno);
#endif
   };

   class TBBBenchmarkRender  : public RenderNode
   //===========================================
   {
   public:
      explicit TBBBenchmarkRender(toMAR::Renderer* renderer, unsigned long camera) :
            repository(Repository::instance()), cameraId(camera), renderer(renderer)
      {}

      uint64_t operator()(uint64_t seqno) const override;

   protected:
      Repository* repository;
      const unsigned long cameraId;
      std::shared_ptr<toMAR::Renderer> renderer;
      static int64_t last_timestamp;
   };

   class TBBNullRenderer : public RenderNode
      //=================================
   {
   public:
      explicit TBBNullRenderer(unsigned long camera, bool isDelete =true) :
      repository(Repository::instance()), cameraId(camera), mustDelete(isDelete)
      {}

      uint64_t operator()(uint64_t seqno) const override;

   private:
      Repository* repository;
      const unsigned long cameraId;
      bool mustDelete;

#ifdef DEBUG_SAVE_TEXTURE
      private:
      static std::thread* debugSaveThread;
      static void debug_threadproc(uint64_t seqno);
#endif
   };

}
#endif
